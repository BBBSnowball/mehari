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
#include <fstream>

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

  if (func.getName() != "evalS")
    return false;

  errs() << "\ngenerate C code for: " << func.getName() << "\n";

  std::vector<Instruction*> worklist;
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  std::string fileName = "_output/code-generation-" + func.getName().str() + ".c";
  
  std::ofstream outputFile(fileName.c_str());
  outputFile << createCCode(worklist);

  return false;
}


std::string SimpleCCodeGenerator::createCCode(std::vector<Instruction*> &instructions) {
  
  enum ParsingState {ALLOCA, STORE, CALC};
  ParsingState pstate = ALLOCA;

  enum CalcState {LOAD_VAR, GET_INDEX, LOAD_INDEX, APPLY};
  CalcState cstate = LOAD_VAR;

  // reset all variables before starting the code generation
  resetVariables();

  std::string ccode;

  for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {

    Instruction *instr = dyn_cast<Instruction>(*instrIt);
    Instruction *prevInstr, *nextInstr;
    if (instrIt != instructions.begin())
      prevInstr = dyn_cast<Instruction>(*(instrIt-1));
    if (instrIt+1 != instructions.end())
      nextInstr = dyn_cast<Instruction>(*(instrIt+1));

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
        addVariable(instr->getOperand(1), paramName);
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
              std::string name = op->getPointerOperand()->getName();
              std::string index;
              if (op->hasIndices())
                // NOTE: the last index is the one we want to know
                for (User::op_iterator it = op->idx_begin(); it != op->idx_end(); ++it)
                  index = dyn_cast<ConstantInt>(*it)->getValue().toString(10, true);
              addVariable(instr, name, index);
            }
            // handle load of local variable
            else {
              // copy name of loaded value to new variable (with current instruction address)
              copyVariable(instr->getOperand(0), instr);
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
            addIndexToVariable(instr->getOperand(0), instr, index);
            
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

            // copy name of loaded value to new variable (with current instruction address)
            copyVariable(instr->getOperand(0), instr);

            if (isa<LoadInst>(nextInstr))
              cstate = LOAD_VAR;
            else
              cstate = APPLY;
          }
          break;


          case APPLY:
          {

            if (isa<StoreInst>(instr)) {
              ccode += printStore(instr->getOperand(1), instr->getOperand(0));
            }
            else if (isa<BinaryOperator>(instr)) {
              std::string tmpVar = createTemporaryVariable(instr, "double");
              ccode += printBinaryOperator(tmpVar, instr->getOperand(0), instr->getOperand(1), instr->getOpcodeName());
            }
            else if (CallInst *sInstr = dyn_cast<CallInst>(instr)) {
              std::string tmpVar = createTemporaryVariable(instr, "double");
              Function *func = sInstr->getCalledFunction();
              ccode += printCall(tmpVar, sInstr->getOperand(0), func->getName().str());
            }
            else if (CmpInst *cmpInstr = dyn_cast<CmpInst>(instr)) {
              std::string tmpVar = createTemporaryVariable(instr, "double");
              ccode += printComparison(tmpVar, cmpInstr->getOperand(0), cmpInstr->getOperand(1), cmpInstr->getPredicate());
            }
            else if (ZExtInst *extInstr = dyn_cast<ZExtInst>(instr)) {
              std::string tmpVar = createTemporaryVariable(instr, "int");
              ccode +=  printIntegerExtension(tmpVar, extInstr->getOperand(0));
            }
            else if (BranchInst *brInstr = dyn_cast<BranchInst>(instr)) {
              if (brInstr->isUnconditional()) {
                // handle phi nodes branch targets
                if (PHINode *phiInstr = dyn_cast<PHINode>(&brInstr->getSuccessor(0)->front())) {
                  // create temporary variable for phi node assignment before the branch instruction
                  std::string tmpVar = getTemporaryVariable(phiInstr);
                  if (tmpVar.empty())
                    // this is the first time handling this phi node --> create new variable 
                    tmpVar = createTemporaryVariable(phiInstr, "double");
                  // print assignment for the phi node variable
                  ccode += printPhiNodeAssignment(tmpVar, prevInstr);
                }
                // add unconditional branch
                std::string label = createBranchLabel(&brInstr->getSuccessor(0)->front());
                ccode += printUnconditionalBranch(label);                
              }
              else {
                // create conditional branch
                std::string label1 = createBranchLabel(&brInstr->getSuccessor(0)->front());
                std::string label2 = createBranchLabel(&brInstr->getSuccessor(1)->front());
                ccode += printConditionalBranch(brInstr->getCondition(), label1, label2);
              }
            }
            else if (PHINode *phiInstr = dyn_cast<PHINode>(instr)) {
              // here we do not have to print anything, because
              // the variable that is used was already set at the brancht instructions before
            }
            else if (ReturnInst *retInstr = dyn_cast<ReturnInst>(instr)) {
              if (retInstr->getReturnValue() != NULL)
                ccode += printReturn(retInstr->getReturnValue());
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


  // add declarations for temporary variables at the beginning of the code
  ccode = printVariableDeclarations() + "\n" + ccode;

  return ccode;
}


void SimpleCCodeGenerator::resetVariables() {
  variables.clear();
  branchLabels.clear();
  tmpVariables.clear();
  tmpVarNumber = 0;
  tmpLabelNumber = 0;
}


void SimpleCCodeGenerator::addVariable(Value *addr, std::string name) {
  variables[addr] = name;
}

void SimpleCCodeGenerator::addVariable(Value *addr, std::string name, std::string index) {
  index.empty() ? variables[addr] = name : variables[addr] = name + "[" + index + "]";
}

void SimpleCCodeGenerator::copyVariable(Value *source, Value *target) {
  variables[target] = variables[source];
}

void SimpleCCodeGenerator::addIndexToVariable(Value *source, Value *target, std::string index) {
  variables[target] = variables[source] + "[" + index + "]";
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
  // else return the address itself as a string
  else {
    std::stringstream ss;
    ss << addr;
    return "## " + ss.str() + " ##";
  }
}


std::string SimpleCCodeGenerator::createTemporaryVariable(Value *addr, std::string datatype) {
  std::string newTmpVar = "t" + static_cast<std::ostringstream*>( &(std::ostringstream() << tmpVarNumber++) )->str();
  variables[addr] = newTmpVar;
  tmpVariables[datatype].push_back(newTmpVar);
  return newTmpVar;
}

std::string SimpleCCodeGenerator::getTemporaryVariable(Value *addr) {
  if (variables.find(addr) != variables.end())
    return variables[addr];
  else
    return "";
}


std::string SimpleCCodeGenerator::createBranchLabel(Value *target) {
  std::string label = "label" + static_cast<std::ostringstream*>( &(std::ostringstream() << tmpLabelNumber++) )->str();
  branchLabels[target].push_back(label);
  return label;
}


std::string SimpleCCodeGenerator::printStore(Value *op1, Value *op2) {
  return "\t"
    +  getOperandString(op1)
    + " = "
    +  getOperandString(op2)
    +  ";\n";
}

std::string SimpleCCodeGenerator::printBinaryOperator(std::string tmpVar, Value *op1, Value *op2, std::string opcode) {
  return "\t" + tmpVar
    +  " = "
    +  getOperandString(op1)
    +  " " + parseBinaryOperator(opcode) + " "
    +  getOperandString(op2)
    +  ";\n";
}

std::string SimpleCCodeGenerator::printCall(std::string tmpVar, Value *arg, std::string funcName) {
  return "\t" + tmpVar
    +  " = "
    +  funcName
    +  "(" + getOperandString(arg) + ")"
    +  ";\n";
}

std::string SimpleCCodeGenerator::printComparison(std::string tmpVar, Value *op1, Value *op2, unsigned int comparePredicate) {
  return "\t" + tmpVar
    +  " = ("
    +  getOperandString(op1)
    +  " " + parseComparePredicate(comparePredicate) + " "
    +  getOperandString(op2)
    +  ");\n";
}

std::string SimpleCCodeGenerator::printIntegerExtension(std::string tmpVar, Value *op) {
  return "\t" + tmpVar
    +  " = "
    +  "(int)"
    +  getOperandString(op)
    +  ";\n";
}

std::string SimpleCCodeGenerator::printPhiNodeAssignment(std::string tmpVar, Value *op) {
  return "\t" + tmpVar
    +  " = "
    +  getOperandString(op)
    +  ";\n";
}

std::string SimpleCCodeGenerator::printUnconditionalBranch(std::string label) {
  return "\tgoto " + label + ";\n";
}

std::string SimpleCCodeGenerator::printConditionalBranch(Value *condition, std::string label1, std::string label2) {
  return "\tif (" + getOperandString(condition) + ")"
    +  " goto " + label1 + ";"
    +  " else goto " + label2
    + ";\n";
}

std::string SimpleCCodeGenerator::printReturn(Value *retVal) {
  return "\treturn" + getOperandString(retVal) + "\n";
}


std::string SimpleCCodeGenerator::printVariableDeclarations() {
  std::string decl;
  for (std::map<std::string, std::vector<std::string> >::iterator it=tmpVariables.begin(); it!=tmpVariables.end(); ++it) {
    decl += "\t" + it->first;
    for (std::vector<std::string>::iterator varIt = it->second.begin(); varIt != it->second.end(); ++varIt)
      decl += " " + *varIt + ",";
    decl = decl.substr(0, decl.size()-1) + ";\n";
  }
  return decl;
}


void SimpleCCodeGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}


// register pass so we can call it using opt
char SimpleCCodeGenerator::ID = 0;
static RegisterPass<SimpleCCodeGenerator>
Y("ccode", "Generate simple C code from LLVM IR. This will only work for especially preparted sources.");
