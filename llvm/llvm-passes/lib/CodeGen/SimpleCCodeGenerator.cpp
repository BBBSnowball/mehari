#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/ADT/APInt.h"

#include "llvm/IR/Operator.h"

#include <vector>
#include <string>
#include <sstream>

using namespace llvm;


SimpleCCodeGenerator::SimpleCCodeGenerator() : FunctionPass(ID) {
  // TODO: complete and check operator and compare predicate mappings
  binaryOperatorStrings["fadd"] = "+";
  binaryOperatorStrings["fsub"] = "-";
  binaryOperatorStrings["fmul"] = "*";
  binaryOperatorStrings["fdiv"] = "/";
  binaryOperatorStrings["or"] = "|";

  comparePredicateStrings[1]  = "==";
  comparePredicateStrings[2]  = ">";
  comparePredicateStrings[3]  = ">=";
  comparePredicateStrings[4]  = "<";
  comparePredicateStrings[5]  = "<=";
  comparePredicateStrings[33] = "!=";
}

SimpleCCodeGenerator::~SimpleCCodeGenerator() {}


bool SimpleCCodeGenerator::runOnFunction(Function &func) {

  // TODO: add parameter to define the IR code that will be transformed to C code
  if (func.getName() != "evalS")
    return false;

  std::vector<Instruction*> worklist;
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  errs() << "\n\ngenerate C code for: " << func.getName() << "\n";
  errs() << "--------------------\n";
  errs() << createCCode(worklist);

  return false;
}


std::string SimpleCCodeGenerator::createCCode(std::vector<Instruction*> &instructions) {
  
  enum ParsingState {ALLOCA, STORE, CALC};
  ParsingState pstate = ALLOCA;

  enum CalcState {LOAD_VAR, GET_INDEX, LOAD_INDEX, APPLY};
  CalcState cstate = LOAD_VAR;

  unsigned int tmpVarNumber = 0;
  unsigned int tmpLabelNumber = 0;

  std::string ccode;
  std::map<std::string, std::vector<std::string> > tmpVariables;

  for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {

    Instruction *instr = dyn_cast<Instruction>(*instrIt);
    Instruction *prevInstr, *nextInstr;
    if (instrIt != instructions.begin())
      prevInstr = dyn_cast<Instruction>(*(instrIt-1));
    if (instrIt+1 != instructions.end())
      nextInstr = dyn_cast<Instruction>(*(instrIt+1));

    // DEBUG
    // errs() << *instr << " ( " << instr << " )\n";

    switch (pstate) {

      case ALLOCA:
        if (isa<StoreInst>(nextInstr))
          pstate = STORE;
      break;

      case STORE:
      {
        // read parameter values and save them in variable mapping
        std::string paramName = instr->getOperand(0)->getName();
        // TODO: very quick and dirty -> find a better solution for this!
        if (paramName == "status")
          paramName = "*status";
        variables[instr->getOperand(1)] = paramName;
        if (!isa<StoreInst>(nextInstr)) {
          pstate = CALC;
        }
      }
      break;

      case CALC:   
      {
        // insert a label for branch targets if the current instruction is one
        if (branchLabels.find(instr) != branchLabels.end()) {
          std::vector<std::string> labels = branchLabels[instr];
          for (std::vector<std::string>::iterator it = labels.begin(); it != labels.end(); ++it)
            ccode += *it + ":\n";
        }

        switch (cstate) {

          case LOAD_VAR:
          {
            if (!isa<LoadInst>(instr))
              errs() << "ERROR: Expected load instruction but there an instruction of a different type!\n";

            // handle load of global variable
            if (GEPOperator *op = dyn_cast<GEPOperator>(dyn_cast<LoadInst>(instr)->getOperand(0))) {
              std::string index;
              if (op->hasIndices())
                // NOTE: the last index is the one we want to know
                for (User::op_iterator it = op->idx_begin(); it != op->idx_end(); ++it)
                  index = dyn_cast<ConstantInt>(*it)->getValue().toString(10, true);
              std::string name = op->getPointerOperand()->getName();
              index.empty() ? variables[instr] = name : variables[instr] = name + "[" + index + "]";
            }
            // handle load of local variable
            else {
              variables[instr] = variables[instr->getOperand(0)];
            }

            if (isa<GetElementPtrInst>(nextInstr))
              cstate = GET_INDEX;
            else if (isa<LoadInst>(nextInstr))
              cstate = LOAD_VAR;
            else
              cstate = APPLY;
          }
          break;


          case GET_INDEX:
          {
            if (!isa<GetElementPtrInst>(instr))
              errs() << "ERROR: Expected getelementptr instruction but there an instruction of a different type!\n";

            std::string index = dyn_cast<llvm::ConstantInt>(instr->getOperand(1))->getValue().toString(10, true);
            variables[instr] = variables[instr->getOperand(0)] + "[" + index + "]";
            
            if (isa<LoadInst>(nextInstr))
              cstate = LOAD_INDEX;
            else
              cstate = APPLY;
          }
          break;


          case LOAD_INDEX:
          {
            if (!isa<LoadInst>(instr))
              errs() << "ERROR: Expected load instruction but there an instruction of a different type!\n";

            variables[instr] = variables[instr->getOperand(0)];

            if (isa<LoadInst>(nextInstr))
              cstate = LOAD_VAR;
            else
              cstate = APPLY;
          }
          break;


          case APPLY:
          {
            // DEBUG
            // for (User::op_iterator it = instr->op_begin(); it != instr->op_end(); ++it) {
            //   errs() << "   - " << getOperandString(*it) << "\n";
            // }

            if (isa<StoreInst>(instr)) {
              ccode += "\t"
                    +  getOperandString(instr->getOperand(1))
                    +  " = "
                    +  getOperandString(instr->getOperand(0))
                    +  ";\n";
            }
            else if (isa<BinaryOperator>(instr)) {
              std::string varNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpVarNumber) )->str();
              ccode += "\tt" + varNumber
                    +  " = "
                    +  getOperandString(instr->getOperand(0))
                    +  " " + parseBinaryOperator(instr->getOpcodeName()) + " "
                    +  getOperandString(instr->getOperand(1))
                    +  ";\n";
              variables[instr] = "t" + varNumber;
              tmpVariables["double"].push_back("t" + varNumber);
              tmpVarNumber++;
            }
            else if (CallInst *sInstr = dyn_cast<CallInst>(instr)) {
              std::string varNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpVarNumber) )->str();
              Function *func = sInstr->getCalledFunction();
              ccode += "\tt"
                    + varNumber
                    +  " = "
                    +  func->getName().str()
                    +  "(" + getOperandString(sInstr->getOperand(0)) + ")"
                    +  ";\n";
              variables[instr] = "t" + varNumber;
              tmpVariables["double"].push_back("t" + varNumber);
              tmpVarNumber++;
            }
            else if (BranchInst *brInstr = dyn_cast<BranchInst>(instr)) {
              if (brInstr->isUnconditional()) {
                // handle phi nodes branch targets
                if (PHINode *phiInstr = dyn_cast<PHINode>(&brInstr->getSuccessor(0)->front())) {
                  // create temporary variable for phi node assignment
                  if (phiNodeOperands.find(phiInstr) == phiNodeOperands.end()) {
                    // this is the first time handling this phi node --> create new variable 
                    std::string varNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpVarNumber) )->str();
                    ccode += "\tt"
                          +  varNumber
                          +  " = "
                          +  getOperandString(prevInstr)
                          +  ";\n";
                    phiNodeOperands[phiInstr] = "t" + varNumber;
                    tmpVariables["double"].push_back("t" + varNumber);
                    tmpVarNumber++;
                  }
                  else {
                    // this is the second time handling this phi node --> reuse the variable
                    ccode += "\t"
                          +  phiNodeOperands[phiInstr]
                          +  " = "
                          +  getOperandString(prevInstr)
                          +  ";\n";
                  }
                }
                std::string labelNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpLabelNumber) )->str();
                ccode += "\tgoto label" + labelNumber + ";\n";
                // save instruction to be labeled 
                branchLabels[&brInstr->getSuccessor(0)->front()].push_back("label" + labelNumber);
                tmpLabelNumber++;
              }
              else {
                std::string labelNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpLabelNumber) )->str();
                std::string labelNumber2 = static_cast<std::ostringstream*>( &(std::ostringstream() << (tmpLabelNumber+1)) )->str();
                ccode += "\tif (" + getOperandString(brInstr->getCondition()) + ")" 
                      +  " goto label" + labelNumber + ";"
                      +  " else goto label" + (labelNumber2)
                      + ";\n";
                // save instruction to be labeled 
                branchLabels[&brInstr->getSuccessor(0)->front()].push_back("label" + labelNumber);
                branchLabels[&brInstr->getSuccessor(1)->front()].push_back("label" + labelNumber2);
                tmpLabelNumber += 2;
              }
            }
            else if (CmpInst *cmpInstr = dyn_cast<CmpInst>(instr)) {
              std::string varNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpVarNumber) )->str();
              ccode += "\tt" + varNumber
                    +  " = ("
                    +  getOperandString(cmpInstr->getOperand(0))
                    +  " " + parseComparePredicate(cmpInstr->getPredicate()) + " "
                    +  getOperandString(cmpInstr->getOperand(1))
                    +  ");\n";
              variables[instr] = "t" + varNumber;
              tmpVariables["int"].push_back("t" + varNumber);
              tmpVarNumber++;
            }
            else if (PHINode *phiInstr = dyn_cast<PHINode>(instr)) {
              // here we do not have to print anything because
              // the variable that should be used was set at the brancht instructions before
            }
            else if (ZExtInst *extInstr = dyn_cast<ZExtInst>(instr)) {
              std::string varNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << tmpVarNumber) )->str();
              ccode +=  "\tt" + varNumber
                    +  " = "
                    +  "(int)"
                    +  getOperandString(extInstr->getOperand(0))
                    +  ";\n";
              variables[instr] = "t" + varNumber;
              tmpVariables["int"].push_back("t" + varNumber);
              tmpVarNumber++;
            }
            else
              errs() << "ERROR: unhandled instruction type: " << *instr << "\n";
            
            
            if (isa<LoadInst>(nextInstr))
              cstate = LOAD_VAR;

          }
          break; // case APPLY
        } // switch cstate
      }
      break; // case CALC
    } // switch pstate     
  } // for instructions

  // create declarations for temporary variables
  std::string decl;
  for (std::map<std::string, std::vector<std::string> >::iterator it=tmpVariables.begin(); it!=tmpVariables.end(); ++it) {
    decl += "\t" + it->first;
    for (std::vector<std::string>::iterator varIt = it->second.begin(); varIt != it->second.end(); ++varIt) {
      decl += " " + *varIt + ",";
    }
    decl = decl.substr(0, decl.size()-1) + ";\n";
  }
  ccode = decl + "\n" + ccode;

  return ccode;
}


std::string SimpleCCodeGenerator::parseBinaryOperator(std::string opcode) {
  if (binaryOperatorStrings.find(opcode) != binaryOperatorStrings.end())
    return binaryOperatorStrings[opcode];
  else
    return "<binary operator " + opcode + ">";
}

std::string SimpleCCodeGenerator::parseComparePredicate(unsigned int predicateNumber) {
  if (comparePredicateStrings.find(predicateNumber) != comparePredicateStrings.end())
    return comparePredicateStrings[predicateNumber];
  else
    return "<compare predicate " + static_cast<std::ostringstream*>( &(std::ostringstream() << predicateNumber) )->str() + ">";
}


std::string SimpleCCodeGenerator::getOperandString(Value* addr) {
  // return operand if it is a variable and already known
  if (variables.find(addr) != variables.end())
    return variables[addr];
  // return operand if it was a phi node
  if (phiNodeOperands.find(addr) != phiNodeOperands.end())
    return phiNodeOperands[addr];
  // handle global value operands
  else if (GEPOperator *op = dyn_cast<GEPOperator>(addr)) {
    std::string index;
    if (op->hasIndices())
      // NOTE: the last index is the one we want to know
      for (User::op_iterator it = op->idx_begin(); it != op->idx_end(); ++it)
        index = dyn_cast<ConstantInt>(*it)->getValue().toString(10, true);
    std::string name = op->getPointerOperand()->getName();
    std::string operand;
    index.empty() ? operand = name : operand = name + "[" + index + "]"; 
    variables[addr] = operand;
    return operand;
  }
  // handle constant integers
  else if (ConstantInt *ci = dyn_cast<ConstantInt>(addr))
    return ci->getValue().toString(10, true);
  // handle constant floating point numbers
  else if (ConstantFP *cf = dyn_cast<ConstantFP>(addr)) {
    double value = cf->getValueAPF().convertToDouble();
    return static_cast<std::ostringstream*>( &(std::ostringstream() << value) )->str();
  }
  // else return the address string itself
  else {
    std::stringstream ss;
    ss << addr;
    return "## " + ss.str() + " ##";
  }
}


void SimpleCCodeGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}


// register pass so we can call it using opt
char SimpleCCodeGenerator::ID = 0;
static RegisterPass<SimpleCCodeGenerator>
Y("ccode", "Generate simple C code from LLVM IR. This will only work for especially preparted sources.");
