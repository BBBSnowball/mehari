#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Operator.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <fstream>

#include <boost/scoped_ptr.hpp>

#include "mehari/utils/MapUtils.h"
#include "mehari/utils/StringUtils.h"

using namespace llvm;

struct CCodeMaps {
  std::map<std::string, std::string> binaryOperatorStrings;
  std::map<FCmpInst::Predicate, std::string> comparePredicateStrings;

  CCodeMaps() {
    // TODO: complete and check operator and compare predicate mappings
    binaryOperatorStrings["fadd"] = "+";
    binaryOperatorStrings["add"]  = "+";
    binaryOperatorStrings["fsub"] = "-";
    binaryOperatorStrings["sub"]  = "-";
    binaryOperatorStrings["fmul"] = "*";
    binaryOperatorStrings["mul"]  = "*";
    binaryOperatorStrings["fdiv"] = "/";
    binaryOperatorStrings["div"]  = "/";
    binaryOperatorStrings["or"]   = "|";
    binaryOperatorStrings["and"]  = "&";

    // not all of these cases are implemented in the code generator
    // isnan: at least one of the arguments is a not-a-number value
    comparePredicateStrings[FCmpInst::FCMP_FALSE] = "false";
    comparePredicateStrings[FCmpInst::FCMP_OEQ]   = "==";
    comparePredicateStrings[FCmpInst::FCMP_OGT]   = ">";
    comparePredicateStrings[FCmpInst::FCMP_OGE]   = ">=";
    comparePredicateStrings[FCmpInst::FCMP_OLT]   = "<";
    comparePredicateStrings[FCmpInst::FCMP_OLE]   = "<=";
    comparePredicateStrings[FCmpInst::FCMP_ONE]   = "!=";
    comparePredicateStrings[FCmpInst::FCMP_ORD]   = "!isnan";
    comparePredicateStrings[FCmpInst::FCMP_UNO]   = "isnan";
    comparePredicateStrings[FCmpInst::FCMP_UEQ]   = "isnan || ==";
    comparePredicateStrings[FCmpInst::FCMP_UGT]   = "isnan || >";
    comparePredicateStrings[FCmpInst::FCMP_UGE]   = "isnan || >=";
    comparePredicateStrings[FCmpInst::FCMP_ULT]   = "isnan || <";
    comparePredicateStrings[FCmpInst::FCMP_ULE]   = "isnan || <=";
    comparePredicateStrings[FCmpInst::FCMP_UNE]   = "isnan || !=";
    comparePredicateStrings[FCmpInst::FCMP_TRUE]  = "true";
    comparePredicateStrings[FCmpInst::ICMP_EQ]    = "==";
    comparePredicateStrings[FCmpInst::ICMP_NE]    = "!=";
    comparePredicateStrings[FCmpInst::ICMP_UGT]   = ">";
    comparePredicateStrings[FCmpInst::ICMP_UGE]   = ">=";
    comparePredicateStrings[FCmpInst::ICMP_ULT]   = "<";
    comparePredicateStrings[FCmpInst::ICMP_ULE]   = "<=";
    comparePredicateStrings[FCmpInst::ICMP_SGT]   = ">";
    comparePredicateStrings[FCmpInst::ICMP_SGE]   = ">=";
    comparePredicateStrings[FCmpInst::ICMP_SLT]   = "<";
    comparePredicateStrings[FCmpInst::ICMP_SLE]   = "<=";
  }

  static const CCodeMaps* instance() {
    if (!_instance)
      _instance.reset(new CCodeMaps());
    return _instance.get();
  }

  static std::string parseBinaryOperator(std::string opcode) {
    return getValueOrDefault(instance()->binaryOperatorStrings, opcode, "<binary operator " + opcode + ">");
  }

  static std::string parseComparePredicate(FCmpInst::Predicate predicateNumber) {
    return getValueOrDefault(instance()->comparePredicateStrings, predicateNumber,
      std::string("<compare predicate ") + predicateNumber + ">");
  }

private:
  static boost::scoped_ptr<CCodeMaps> _instance;
};

boost::scoped_ptr<CCodeMaps> CCodeMaps::_instance;


SimpleCCodeGenerator::SimpleCCodeGenerator(CodeGeneratorBackend* backend)
    : tmpVarNameGenerator("t"), backend(backend ? backend : new CCodeBackend()) { }

SimpleCCodeGenerator::~SimpleCCodeGenerator() {}


void SimpleCCodeGenerator::createCCode(std::ostream& stream, Function &func, std::vector<Instruction*> &instructions) {
  
  enum CalcState {LOAD_VAR, GET_INDEX, LOAD_INDEX, APPLY};
  CalcState cstate;

  std::stringstream ccode;

  // reset all variables before starting the code generation
  resetVariables();

  // extract the parameter information from the given function
  extractFunctionParameters(func);

  // determine the CalcState to start with
  Instruction *firstInstr = instructions.front();
  if (isa<LoadInst>(firstInstr)) 
    cstate = LOAD_VAR;
  else
    cstate = APPLY;


  // generate C code for the given instruction vector
  for (std::vector<Instruction*>::iterator instrIt = instructions.begin();
      instrIt != instructions.end(); ++instrIt) {

    Instruction *instr = dyn_cast<Instruction>(*instrIt);
    Instruction *prevInstr, *nextInstr;
    if (instrIt != instructions.begin())
      prevInstr = dyn_cast<Instruction>(*(instrIt-1));
    if (instrIt+1 != instructions.end())
      nextInstr = dyn_cast<Instruction>(*(instrIt+1));

    // insert a label for branch targets if the current instruction is one
    backend->generateBranchTargetIfNecessary(ccode, instr);

    switch (cstate) {

      case LOAD_VAR:
      {
        if (!isa<LoadInst>(instr)) {
          errs() << "ERROR: Expected load instruction but there is an instruction of a different type!\n";
          break;
        }

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
        if (!isa<GetElementPtrInst>(instr)) {
          errs() << "ERROR: Expected getelementptr instruction but there an instruction of a different type!\n";
          break;
        }

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
        if (!isa<LoadInst>(instr)) {
          errs() << "ERROR: Expected load instruction but there an instruction of a different type!\n";
          break;
        }

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
          backend->generateStore(ccode, instr->getOperand(1), instr->getOperand(0));
        }
        else if (isa<BinaryOperator>(instr)) {
          std::string opcode = instr->getOpcodeName();
          // create new temporary variable and print C code
          std::string tmpVar = createTemporaryVariable(instr, getDatatype(instr));
          backend->generateBinaryOperator(ccode, tmpVar, instr->getOperand(0), instr->getOperand(1), opcode);
        }
        else if (CallInst *cInstr = dyn_cast<CallInst>(instr)) {
          Function *func = cInstr->getCalledFunction();
          std::string functionName = func->getName().str();
          std::vector<Value*> args;
          for (int i=0; i<cInstr->getNumArgOperands(); i++)
            args.push_back(cInstr->getArgOperand(i));
          if (func->getReturnType()->isVoidTy()) {
            backend->generateVoidCall(ccode, functionName, args);
          }
          else {
            std::string tmpVar = createTemporaryVariable(instr, getDatatype(instr));
            backend->generateCall(ccode, functionName, tmpVar, args);
            // TODO: not very nice.. is there a better solution without asking for the function name?
            if (functionName == "_get_real" || functionName == "_get_int" 
             || functionName == "_get_bool" || functionName == "_get_intptr") {
              std::string tgtOperand = cast<MDString>(instr->getMetadata("targetop")->getOperand(0))->getString();
              dataDependencies[tgtOperand] = tmpVar;
            }
          }
        }
        else if (CmpInst *cmpInstr = dyn_cast<CmpInst>(instr)) {
          std::string tmpVar = createTemporaryVariable(instr, getDatatype(instr));
          backend->generateComparison(ccode, tmpVar, cmpInstr->getOperand(0),
            cmpInstr->getOperand(1), cmpInstr->getPredicate());
        }
        else if (ZExtInst *extInstr = dyn_cast<ZExtInst>(instr)) {
          std::string tmpVar = createTemporaryVariable(instr, getDatatype(instr));
           backend->generateIntegerExtension(ccode, tmpVar, extInstr->getOperand(0));
        }
        else if (BranchInst *brInstr = dyn_cast<BranchInst>(instr)) {
          if (brInstr->isUnconditional()) {
            // handle phi nodes branch targets
            if (PHINode *phiInstr = dyn_cast<PHINode>(&brInstr->getSuccessor(0)->front())) {
              // create temporary variable for phi node assignment before the branch instruction
              std::string tmpVar = getOrCreateTemporaryVariable(phiInstr);
              // print assignment for the phi node variable
              backend->generatePhiNodeAssignment(ccode, tmpVar, prevInstr);
            }
            // add unconditional branch
            std::string label = backend->generateBranchLabel(&brInstr->getSuccessor(0)->front());
            backend->generateUnconditionalBranch(ccode, label);                
          }
          else {
            // create conditional branch
            std::string label1 = backend->generateBranchLabel(&brInstr->getSuccessor(0)->front());
            std::string label2 = backend->generateBranchLabel(&brInstr->getSuccessor(1)->front());
            backend->generateConditionalBranch(ccode, brInstr->getCondition(), label1, label2);
          }
        }
        else if (PHINode *phiInstr = dyn_cast<PHINode>(instr)) {
          // here we do not have to print anything, because
          // the variable that is used was already set at the brancht instructions before
        }
        else if (ReturnInst *retInstr = dyn_cast<ReturnInst>(instr)) {
          if (retInstr->getReturnValue() != NULL)
            backend->generateReturn(ccode, retInstr->getReturnValue());
        }
        else
          errs() << "ERROR: unhandled instruction type: " << *instr << "\n";
        
        
        if (isa<LoadInst>(nextInstr))
          cstate = LOAD_VAR;

      }
      break; // case APPLY

    } // switch cstate

  } // for instructions


  // add declarations for temporary variables at the beginning of the code
  backend->generateVariableDeclarations(stream, tmpVariables);
  stream << ccode.str();
}


void SimpleCCodeGenerator::createExternArray(std::ostream& stream, GlobalArrayVariable &globVar) {
  stream << "extern " << globVar.type << " " << globVar.name 
      << "[" << globVar.numElem << "];\n";
}


void SimpleCCodeGenerator::resetVariables() {
  variables.clear();
  tmpVariables.clear();
  dataDependencies.clear();
  tmpVarNameGenerator.reset();

  backend->init(this);
}


// TODO: this is not very pretty.. find a better solution to get the parameter information
void SimpleCCodeGenerator::extractFunctionParameters(Function &func) {

  // quit if there are no function parameters
  if (func.arg_begin() == func.arg_end())
    return;

  int remainingParams = 0;

  inst_iterator I = inst_begin(func), E = inst_end(func);

  for (; I != E; ++I) {
    Instruction *instr = dyn_cast<Instruction>(&*I);

    if (!isa<AllocaInst>(instr))
      break;

    remainingParams++;
  }

  for (; I != E && remainingParams > 0; ++I) {
    Instruction *instr = dyn_cast<Instruction>(&*I);

    if (!isa<StoreInst>(instr))
      break;

    // read parameter values and save them in variable mapping
    std::string paramName = instr->getOperand(0)->getName();
    // TODO: very quick and dirty -> find a better solution for this!
    if (paramName == "status")
      paramName = "*status";
    addVariable(instr->getOperand(1), paramName);

    remainingParams--;
  }

  if (remainingParams != 0) {
    errs() << "WARNING: Mismatched ALLOCA and STORE instructions in function " << func << "\n";
  }
}


void SimpleCCodeGenerator::addVariable(Value *addr, std::string name) {
  variables[addr] = name;
}

void SimpleCCodeGenerator::addVariable(Value *addr, std::string name, std::string index) {
  variables[addr] = (index.empty() ? name : name + "[" + index + "]");
}

void SimpleCCodeGenerator::copyVariable(Value *source, Value *target) {
  variables[target] = variables[source];
}

void SimpleCCodeGenerator::addIndexToVariable(Value *source, Value *target, std::string index) {
  variables[target] = variables[source] + "[" + index + "]";
}

std::string SimpleCCodeGenerator::getDatatype(Value *addr) {
  return getDatatype(addr->getType());
}

std::string SimpleCCodeGenerator::getDatatype(Type *type) {
  if (type->isPointerTy())
    return getDatatype(type->getPointerElementType()) + " *";
  else if (type->isIntegerTy())
    return "int";
  else if (type->isFloatingPointTy())
    return "double";
  else if (type->isVoidTy())
    return "void";
  else
    return "<unsupported datatype>";
}


std::string SimpleCCodeGenerator::createTemporaryVariable(Value *addr, std::string datatype) {
  std::string newTmpVar = tmpVarNameGenerator.next();
  variables[addr] = newTmpVar;
  tmpVariables[datatype].push_back(newTmpVar);
  return newTmpVar;
}

std::string SimpleCCodeGenerator::getOrCreateTemporaryVariable(Value *addr) {
  if (const std::string* varname = getValueOrNull(variables, addr))
    return *varname;
  else
    return createTemporaryVariable(addr, getDatatype(addr));
}

std::map<Value*, std::string>& SimpleCCodeGenerator::getVariables() {
  return variables;
}

std::string SimpleCCodeGenerator::getDataDependencyOrDefault(std::string opString, std::string defaultValue) {
  return getValueOrDefault(dataDependencies, opString, defaultValue);
}


CCodeBackend::CCodeBackend()
  : branchLabelNameGenerator("label") { }

void CCodeBackend::init(SimpleCCodeGenerator* generator) {
  this->generator = generator;

  branchLabels.clear();
  branchLabelNameGenerator.reset();
}

std::string CCodeBackend::generateBranchLabel(Value *target) {
  std::string label = branchLabelNameGenerator.next();
  branchLabels[target].push_back(label);
  return label;
}


std::string CCodeBackend::getOperandString(Value* addr) {
  // return operand if it is a variable and already known
  if (const std::string* str = getValueOrNull(generator->getVariables(), addr))
    return *str;

  // handle global value operands
  else if (GEPOperator *op = dyn_cast<GEPOperator>(addr)) {
    if (!op->hasAllConstantIndices())
      errs() << "ERROR: Variable indices are not supported!\n";
    std::string indices;
    if (op->hasIndices())
      // NOTE: the last index is the one we want to know
      for (User::op_iterator it = op->idx_begin(); it != op->idx_end(); ++it)
        indices = "[" + dyn_cast<ConstantInt>(*it)->getValue().toString(10, true) + "]";
    std::string name = op->getPointerOperand()->getName();
    std::string operand = name + indices;
    generator->getVariables()[addr] = operand;
    return operand;
  }

  // handle constant integers
  else if (ConstantInt *ci = dyn_cast<ConstantInt>(addr))
    return ci->getValue().toString(10, true);

  // handle constant floating point numbers
  else if (ConstantFP *cf = dyn_cast<ConstantFP>(addr)) {
    double value = cf->getValueAPF().convertToDouble();
    return toString(value);
  }

  // convert operand address to string
  std::string opString = toString(addr);
  // if the value was got from another thread by the get_data function, return it
  return generator->getDataDependencyOrDefault(opString, "## " + opString + " ##");
}


void CCodeBackend::generateStore(std::ostream& stream, Value *op1, Value *op2) {
  stream << "\t"
    <<  getOperandString(op1)
    << " = "
    <<  getOperandString(op2)
    <<  ";\n";
}

void CCodeBackend::generateBinaryOperator(std::ostream& stream, std::string tmpVar,
    Value *op1, Value *op2, std::string opcode) {
  stream << "\t" << tmpVar
    <<  " = "
    <<  getOperandString(op1)
    <<  " " << CCodeMaps::parseBinaryOperator(opcode) << " "
    <<  getOperandString(op2)
    <<  ";\n";
}

void CCodeBackend::generateCall(std::ostream& stream, std::string funcName,
    std::string tmpVar, std::vector<Value*> args) {
  stream << "\t";
  if (!tmpVar.empty())
    stream << tmpVar << " = ";

  stream << funcName << "(";

  Seperator sep(", ");
  for (std::vector<Value*>::iterator it = args.begin(); it != args.end(); ++it) {
    stream << sep << getOperandString(*it);
  }

  stream << ");\n";
}

void CCodeBackend::generateVoidCall(std::ostream& stream, std::string funcName, std::vector<Value*> args) {
  generateCall(stream, funcName, "", args);
}

void CCodeBackend::generateComparison(std::ostream& stream, std::string tmpVar, Value *op1, Value *op2,
    FCmpInst::Predicate comparePredicate) {
  stream << "\t" << tmpVar
    <<  " = ("
    <<  getOperandString(op1)
    <<  " " << CCodeMaps::parseComparePredicate(comparePredicate) << " "
    <<  getOperandString(op2)
    <<  ");\n";
}

void CCodeBackend::generateIntegerExtension(std::ostream& stream, std::string tmpVar, Value *op) {
  stream << "\t" << tmpVar
    <<  " = "
    <<  "(int)"
    <<  getOperandString(op)
    <<  ";\n";
}

void CCodeBackend::generatePhiNodeAssignment(std::ostream& stream, std::string tmpVar, Value *op) {
  stream << "\t" << tmpVar
    <<  " = "
    <<  getOperandString(op)
    <<  ";\n";
}

void CCodeBackend::generateUnconditionalBranch(std::ostream& stream, std::string label) {
  stream << "\tgoto " << label << ";\n";
}

void CCodeBackend::generateConditionalBranch(std::ostream& stream, Value *condition,
    std::string label1, std::string label2) {
  stream << "\tif (" << getOperandString(condition) << ")"
    <<  " goto " << label1 << ";"
    <<  " else goto " << label2
    << ";\n";
}

void CCodeBackend::generateReturn(std::ostream& stream, Value *retVal) {
  stream << "\treturn " << getOperandString(retVal) << ";\n";
}


void CCodeBackend::generateVariableDeclarations(std::ostream& stream,
    const std::map<std::string, std::vector<std::string> >& tmpVariables) {
  for (std::map<std::string, std::vector<std::string> >::const_iterator it=tmpVariables.begin();
      it!=tmpVariables.end(); ++it) {
    stream << "\t" << it->first << " ";

    Seperator sep(", ");
    for (std::vector<std::string>::const_iterator varIt = it->second.begin(); varIt != it->second.end(); ++varIt)
      stream << sep << *varIt;

    stream << ";\n";
  }
}

void CCodeBackend::generateBranchTargetIfNecessary(std::ostream& stream, llvm::Instruction* instr) {
  llvm::Value* instr_as_value = instr;
  if (std::vector<std::string>* labels = getValueOrNull(branchLabels, instr_as_value)) {
    for (std::vector<std::string>::iterator it = labels->begin(); it != labels->end(); ++it)
      stream << *it << ":\n";
  }
}
