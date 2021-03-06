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
#include <iomanip>
#include <fstream>

#include <boost/scoped_ptr.hpp>
#include <boost/assign.hpp>

#include "mehari/utils/ContainerUtils.h"
#include "mehari/utils/StringUtils.h"

using namespace llvm;

namespace CCodeMaps {
  std::map<unsigned, std::string> binaryOperatorStrings = boost::assign::map_list_of
    // TODO: complete and check operator and compare predicate mappings
    (Instruction::FAdd, "+")
    (Instruction::Add,  "+")
    (Instruction::FSub, "-")
    (Instruction::Sub,  "-")
    (Instruction::FMul, "*")
    (Instruction::Mul,  "*")
    (Instruction::FDiv, "/")
    (Instruction::UDiv, "/")
    (Instruction::SDiv, "/")
    (Instruction::URem, "%")
    (Instruction::SRem, "%")
    (Instruction::FRem, "%")
    (Instruction::Or,   "|")
    (Instruction::And,  "&");

    // not all of these cases are implemented in the code generator
    // isnan: at least one of the arguments is a not-a-number value
  std::map<FCmpInst::Predicate, std::string> comparePredicateStrings = boost::assign::map_list_of
    (FCmpInst::FCMP_FALSE, "false")
    (FCmpInst::FCMP_OEQ,   "==")
    (FCmpInst::FCMP_OGT,   ">")
    (FCmpInst::FCMP_OGE,   ">=")
    (FCmpInst::FCMP_OLT,   "<")
    (FCmpInst::FCMP_OLE,   "<=")
    (FCmpInst::FCMP_ONE,   "!=")
    (FCmpInst::FCMP_ORD,   "!isnan")
    (FCmpInst::FCMP_UNO,   "isnan")
    (FCmpInst::FCMP_UEQ,   "isnan || ==")
    (FCmpInst::FCMP_UGT,   "isnan || >")
    (FCmpInst::FCMP_UGE,   "isnan || >=")
    (FCmpInst::FCMP_ULT,   "isnan || <")
    (FCmpInst::FCMP_ULE,   "isnan || <=")
    (FCmpInst::FCMP_UNE,   "isnan || !=")
    (FCmpInst::FCMP_TRUE,  "true")
    (FCmpInst::ICMP_EQ,    "==")
    (FCmpInst::ICMP_NE,    "!=")
    (FCmpInst::ICMP_UGT,   ">")
    (FCmpInst::ICMP_UGE,   ">=")
    (FCmpInst::ICMP_ULT,   "<")
    (FCmpInst::ICMP_ULE,   "<=")
    (FCmpInst::ICMP_SGT,   ">")
    (FCmpInst::ICMP_SGE,   ">=")
    (FCmpInst::ICMP_SLT,   "<")
    (FCmpInst::ICMP_SLE,   "<=");

  std::string parseBinaryOperator(unsigned opcode) {
    return getValueOrDefault(binaryOperatorStrings, opcode,
      std::string("<binary operator ") + Instruction::getOpcodeName(opcode) + ">");
  }

  static std::string parseComparePredicate(FCmpInst::Predicate predicateNumber) {
    return getValueOrDefault(comparePredicateStrings, predicateNumber,
      std::string("<compare predicate ") + predicateNumber + ">");
  }
}


SimpleCCodeGenerator::SimpleCCodeGenerator(CodeGeneratorBackend* backend)
    : ignoreDataDependencies(false),
      backend(backend ? backend : new CCodeBackend()) { }

SimpleCCodeGenerator::~SimpleCCodeGenerator() {
  if (backend)
    delete backend;
}

void SimpleCCodeGenerator::setIgnoreDataDependencies(bool ignoreThem) {
  ignoreDataDependencies = ignoreThem;
}

template <typename T>
T* convertAddressStringToPointer(const std::string& address) {
  std::stringstream ss;
  assert(address.substr(0, 2) == "0x");
  ss << address.substr(2);
  size_t tmp(0);
  if(!(ss >> std::hex >> tmp))
   throw "Failed - invalid address!";
  return reinterpret_cast<T*>(tmp);
}

void SimpleCCodeGenerator::createCCode(std::ostream& stream, Function &func, const std::vector<Instruction*> &instructions) {
  
  enum CalcState {LOAD_VAR, GET_INDEX, LOAD_INDEX, APPLY};
  CalcState cstate;

  // reset all variables before starting the code generation
  resetVariables();
  backend->init(this, stream);

  // extract the parameter information from the given function
  extractFunctionParameters(func);

  // determine the CalcState to start with
  Instruction *firstInstr = instructions.front();
  if (isa<LoadInst>(firstInstr)) 
    cstate = LOAD_VAR;
  else
    cstate = APPLY;


  // generate C code for the given instruction vector
  for (std::vector<Instruction*>::const_iterator instrIt = instructions.begin();
      instrIt != instructions.end(); ++instrIt) {

    Instruction *instr = dyn_cast<Instruction>(*instrIt);
    Instruction *nextInstr;
    if (instrIt+1 != instructions.end())
      nextInstr = dyn_cast<Instruction>(*(instrIt+1));
    else
      nextInstr = NULL;

    // insert a label for branch targets if the current instruction is one
    backend->generateBranchTargetIfNecessary(instr);

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
          backend->addVariable(instr, name, index);
        }
        // handle load of local variable
        else {
          // copy name of loaded value to new variable (with current instruction address)
          backend->copyVariable(instr->getOperand(0), instr);
        }

        if (nextInstr && isa<GetElementPtrInst>(nextInstr))
          cstate = GET_INDEX;
        else if (nextInstr && isa<LoadInst>(nextInstr))
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
        backend->addIndexToVariable(instr->getOperand(0), instr, index);
        
        if (nextInstr && isa<LoadInst>(nextInstr))
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
        backend->copyVariable(instr->getOperand(0), instr);

        if (nextInstr && isa<LoadInst>(nextInstr))
          cstate = LOAD_VAR;
        else
          cstate = APPLY;
      }
      break;


      case APPLY:
      {

        if (isa<StoreInst>(instr)) {
          backend->generateStore(instr->getOperand(1), instr->getOperand(0));
        }
        else if (isa<BinaryOperator>(instr)) {
          // create new temporary variable and print C code
          std::string tmpVar = backend->createTemporaryVariable(instr);
          backend->generateBinaryOperator(tmpVar, instr->getOperand(0), instr->getOperand(1), instr->getOpcode());
        }
        else if (CallInst *cInstr = dyn_cast<CallInst>(instr)) {
          Function *func = cInstr->getCalledFunction();
          std::string functionName = func->getName().str();
          std::vector<Value*> args;
          for (unsigned int i=0; i<cInstr->getNumArgOperands(); i++)
            args.push_back(cInstr->getArgOperand(i));
          if (func->getReturnType()->isVoidTy()) {
            backend->generateVoidCall(functionName, args);
          }
          else {
            std::string tmpVar = backend->createTemporaryVariable(instr);
            backend->generateCall(functionName, tmpVar, args);
            // TODO: not very nice.. is there a better solution without asking for the function name?
            if (functionName == "_get_real" || functionName == "_get_int" 
             || functionName == "_get_bool" || functionName == "_get_intptr") {
              if (!ignoreDataDependencies) {
                std::string tgtOperand = cast<MDString>(instr->getMetadata("targetop")->getOperand(0))->getString();
                dataDependencies[tgtOperand] = tmpVar;

                backend->addDataDependency(convertAddressStringToPointer<Value>(tgtOperand), tmpVar);
              }
            }
          }
        }
        else if (CmpInst *cmpInstr = dyn_cast<CmpInst>(instr)) {
          std::string tmpVar = backend->createTemporaryVariable(instr);
          backend->generateComparison(tmpVar, cmpInstr->getOperand(0),
            cmpInstr->getOperand(1), cmpInstr->getPredicate());
        }
        else if (ZExtInst *extInstr = dyn_cast<ZExtInst>(instr)) {
          std::string tmpVar = backend->createTemporaryVariable(instr);
          backend->generateIntegerExtension(tmpVar, extInstr->getOperand(0));
        }
        else if (BranchInst *brInstr = dyn_cast<BranchInst>(instr)) {
          if (brInstr->isUnconditional()) {
            // handle phi nodes branch targets
            if (PHINode *phiInstr = dyn_cast<PHINode>(&brInstr->getSuccessor(0)->front())) {
              // create temporary variable for phi node assignment before the branch instruction
              std::string tmpVar = backend->getOrCreateTemporaryVariable(phiInstr);
              // read operators of phi node instruction
              assert(phiInstr->getNumIncomingValues() == 2);
              Value *op = NULL;
              for (int i=0; i<2; i++) {
                if (phiInstr->getIncomingBlock(i) == instr->getParent())
                  op = phiInstr->getIncomingValue(i);
              }
              assert(op);
              // print assignment for the phi node variable
              backend->generatePhiNodeAssignment(tmpVar, op);
            }
            // add unconditional branch
            Instruction* target = &brInstr->getSuccessor(0)->front();
            backend->generateUnconditionalBranch(target);                
          }
          else {
            // create conditional branch
            Instruction* targetTrue  = &brInstr->getSuccessor(0)->front();
            Instruction* targetFalse = &brInstr->getSuccessor(1)->front();
            backend->generateConditionalBranch(brInstr->getCondition(), targetTrue, targetFalse);
          }
        }
        else if (isa<PHINode>(instr)) {
          // here we do not have to print anything, because
          // the variable that is used was already set at the branch instructions before
        }
        else if (ReturnInst *retInstr = dyn_cast<ReturnInst>(instr)) {
          if (retInstr->getReturnValue() != NULL)
            backend->generateReturn(retInstr->getReturnValue());
        }
        else if (SelectInst *selInstr = dyn_cast<SelectInst>(instr)) {
          std::string tmpVar = backend->getOrCreateTemporaryVariable(selInstr);
          backend->generateSelect(tmpVar, selInstr->getCondition(), selInstr->getTrueValue(), selInstr->getFalseValue());
        }
        else
          errs() << "ERROR: unhandled instruction type: " << *instr << "\n";
        
        
        if (nextInstr && isa<LoadInst>(nextInstr))
          cstate = LOAD_VAR;

      }
      break; // case APPLY

    } // switch cstate

  } // for instructions

  backend->generateEndOfMethod();
}


void SimpleCCodeGenerator::createExternArray(std::ostream& stream, GlobalArrayVariable &globVar) {
  stream << "extern " << globVar.type << " " << globVar.name 
      << "[" << globVar.numElem << "];\n";
}


void SimpleCCodeGenerator::resetVariables() {
  dataDependencies.clear();
}


// TODO: this is not very pretty.. find a better solution to get the parameter information
void SimpleCCodeGenerator::extractFunctionParameters(Function &func) {

  // quit if there are no function parameters
  if (func.arg_begin() == func.arg_end())
    return;

  inst_iterator I = inst_begin(func), E = inst_end(func);

  for (; I != E; ++I) {
    Instruction *instr = dyn_cast<Instruction>(&*I);

    if (!isa<AllocaInst>(instr))
      break;
  }

  int remainingParams = func.arg_size();
  for (; I != E && remainingParams > 0; ++I, remainingParams--) {
    Instruction *instr = dyn_cast<Instruction>(&*I);

    if (!isa<StoreInst>(instr))
      break;

    // read parameter values and save them in variable mapping
    std::string paramName = instr->getOperand(0)->getName();
    // TODO: very quick and dirty -> find a better solution for this!
    if (paramName == "status")
      paramName = "*status";
    backend->addParameter(instr->getOperand(1), paramName);
  }

  if (remainingParams != 0) {
    errs() << "WARNING: Mismatched ALLOCA and STORE instructions in function " << func << "\n";
  }
}

std::string *SimpleCCodeGenerator::getDataDependencyOrNull(Value *operand) {
  std::string opString = toString(operand);
  return getValueOrNull(dataDependencies, opString);
}


CodeGeneratorBackend::~CodeGeneratorBackend() {}

void CodeGeneratorBackend::generateEndOfMethod() { }

void CodeGeneratorBackend::addDataDependency(Value *valueFromOtherThread, const std::string& isSavedHere) { }


CCodeBackend::CCodeBackend()
  : branchLabelNameGenerator("label"),
    tmpVarNameGenerator("t") { }

CCodeBackend::~CCodeBackend() {}

void CCodeBackend::init(SimpleCCodeGenerator* generator, std::ostream& stream) {
  this->generator = generator;
  this->output_stream = &stream;

  this->ccode.clear();

  branchLabels.clear();
  branchLabelNameGenerator.reset();
  variables.clear();
  tmpVariables.clear();
  tmpVarNameGenerator.reset();
  declarations.str("");
  ccode.str("");
}

std::string CCodeBackend::generateBranchLabel(Value *target) {
  std::string label = branchLabelNameGenerator.next();
  branchLabels[target].push_back(label);
  return label;
}


std::string CCodeBackend::getOperandString(Value* addr) {
  // if the value was got from another thread by the get_data function, return it
  if (std::string *operand = generator->getDataDependencyOrNull(addr))
    return *operand;

  // return operand if it is a variable and already known
  else if (const std::string* str = getValueOrNull(variables, addr))
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
    variables[addr] = operand;
    return operand;
  }

  // handle constant integers
  else if (ConstantInt *ci = dyn_cast<ConstantInt>(addr))
    return ci->getValue().toString(10, true);

  // handle constant floating point numbers
  else if (ConstantFP *cf = dyn_cast<ConstantFP>(addr)) {
    double value = cf->getValueAPF().convertToDouble();

    std::ostringstream os;
    os << std::setprecision(20) << value;
    return os.str();
  }

  else
    return "## " + toString(addr) + " ##";
}


void CCodeBackend::generateStore(Value *op1, Value *op2) {
  ccode << "\t"
    <<  getOperandString(op1)
    << " = "
    <<  getOperandString(op2)
    <<  ";\n";
}

void CCodeBackend::generateBinaryOperator(std::string tmpVar,
    Value *op1, Value *op2, unsigned opcode) {
  ccode << "\t" << tmpVar
    <<  " = "
    <<  getOperandString(op1)
    <<  " " << CCodeMaps::parseBinaryOperator(opcode) << " "
    <<  getOperandString(op2)
    <<  ";\n";
}

void CCodeBackend::generateCall(std::string funcName,
    std::string tmpVar, std::vector<Value*> args) {
  ccode << "\t";
  if (!tmpVar.empty())
    ccode << tmpVar << " = ";

  ccode << funcName << "(";

  Seperator sep(", ");
  for (std::vector<Value*>::iterator it = args.begin(); it != args.end(); ++it) {
    ccode << sep << getOperandString(*it);
  }

  ccode << ");\n";
}

void CCodeBackend::generateVoidCall(std::string funcName, std::vector<Value*> args) {
  generateCall(funcName, "", args);
}

void CCodeBackend::generateComparison(std::string tmpVar, Value *op1, Value *op2,
    FCmpInst::Predicate comparePredicate) {
  ccode << "\t" << tmpVar
    <<  " = ("
    <<  getOperandString(op1)
    <<  " " << CCodeMaps::parseComparePredicate(comparePredicate) << " "
    <<  getOperandString(op2)
    <<  ");\n";
}

void CCodeBackend::generateIntegerExtension(std::string tmpVar, Value *op) {
  ccode << "\t" << tmpVar
    <<  " = "
    <<  "(int)"
    <<  getOperandString(op)
    <<  ";\n";
}

void CCodeBackend::generateSelect(std::string tmpVar, Value *condition, Value *targetTrue, Value *targetFalse) {
    ccode << "\t" << tmpVar
    <<  " = "
    << getOperandString(condition) << "? "
    << getOperandString(targetTrue) << " : "
    << getOperandString(targetFalse)
    << ";\n";
}

void CCodeBackend::generatePhiNodeAssignment(std::string tmpVar, Value *op) {
  ccode << "\t" << tmpVar
    <<  " = "
    <<  getOperandString(op)
    <<  ";\n";
}

void CCodeBackend::generateUnconditionalBranch(std::string label) {
  ccode << "\tgoto " << label << ";\n";
}

void CCodeBackend::generateConditionalBranch(Value *condition,
    std::string label1, std::string label2) {
  ccode << "\tif (" << getOperandString(condition) << ")"
    <<  " goto " << label1 << ";"
    <<  " else goto " << label2
    << ";\n";
}

void CCodeBackend::generateUnconditionalBranch(Instruction *target) {
  generateUnconditionalBranch(generateBranchLabel(target));
}
void CCodeBackend::generateConditionalBranch(Value *condition, Instruction *targetTrue, Instruction *targetFalse) {
  generateConditionalBranch(condition, generateBranchLabel(targetTrue), generateBranchLabel(targetFalse));
}

void CCodeBackend::generateReturn(Value *retVal) {
  ccode << "\treturn " << getOperandString(retVal) << ";\n";
}


void CCodeBackend::generateVariableDeclarations() {
  declarations.str("");

  for (std::map<std::string, std::vector<std::string> >::const_iterator it=tmpVariables.begin();
      it!=tmpVariables.end(); ++it) {
    declarations << "\t" << it->first << " ";

    Seperator sep(", ");
    for (std::vector<std::string>::const_iterator varIt = it->second.begin(); varIt != it->second.end(); ++varIt)
      declarations << sep << *varIt;

    declarations << ";\n";
  }
}

void CCodeBackend::generateBranchTargetIfNecessary(llvm::Instruction* instr) {
  llvm::Value* instr_as_value = instr;
  if (std::vector<std::string>* labels = getValueOrNull(branchLabels, instr_as_value)) {
    for (std::vector<std::string>::iterator it = labels->begin(); it != labels->end(); ++it)
      ccode << *it << ":\n";
  }
}

void CCodeBackend::generateEndOfMethod() {
  // add declarations for temporary variables at the beginning of the code
  generateVariableDeclarations();
  *output_stream << declarations.str() << ccode.str();
}

std::string CCodeBackend::createTemporaryVariable(Value *addr) {
  std::string datatype = getDatatype(addr);
  std::string newTmpVar = tmpVarNameGenerator.next();
  variables[addr] = newTmpVar;
  tmpVariables[datatype].push_back(newTmpVar);
  return newTmpVar;
}

std::string CCodeBackend::getOrCreateTemporaryVariable(Value *addr) {
  if (const std::string* varname = getValueOrNull(variables, addr))
    return *varname;
  else
    return createTemporaryVariable(addr);
}

void CCodeBackend::addVariable(Value *addr, std::string name) {
  variables[addr] = name;
}

void CCodeBackend::addParameter(Value *addr, std::string name) {
  addVariable(addr, name);
}

void CCodeBackend::addVariable(Value *addr, std::string name, std::string index) {
  variables[addr] = (index.empty() ? name : name + "[" + index + "]");
}

void CCodeBackend::copyVariable(Value *source, Value *target) {
  variables[target] = variables[source];
}

void CCodeBackend::addIndexToVariable(Value *source, Value *target, std::string index) {
  variables[target] = variables[source] + "[" + index + "]";
}

std::string CCodeBackend::getDatatype(Value *addr) {
  return getDatatype(addr->getType());
}

std::string CCodeBackend::getDatatype(Type *type) {
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
