#ifndef SIMPLE_CCODE_GENERATOR_H
#define SIMPLE_CCODE_GENERATOR_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

#include <string>
#include <vector>
#include <map>

using namespace llvm;

class CodeGeneratorBackend;

class SimpleCCodeGenerator {

public:
  SimpleCCodeGenerator();
  ~SimpleCCodeGenerator();

  typedef struct {
    std::string name;
    std::string type;
    unsigned int numElem;
  } GlobalArrayVariable;

  std::string createCCode(Function &func, std::vector<Instruction*> &instructions);
  std::string createExternArray(GlobalArrayVariable &globVar);


  std::string parseBinaryOperator(std::string opcode);
  std::string parseComparePredicate(FCmpInst::Predicate predicateNumber);

  std::string getOperandString(Value* addr);

private:
  std::map<Value*, std::string> variables;
  std::map<std::string, std::vector<std::string> > tmpVariables;
  std::map<std::string, std::string> dataDependencies;
  unsigned int tmpVarNumber;
  
  std::map<std::string, std::string> binaryOperatorStrings;
  std::map<FCmpInst::Predicate, std::string> comparePredicateStrings;

  CodeGeneratorBackend* backend;

  void resetVariables();

  void extractFunctionParameters(Function &func);

  void addVariable(Value *addr, std::string name);
  void addVariable(Value *addr, std::string name, std::string index);
  void copyVariable(Value *source, Value *target);
  void addIndexToVariable(Value *source, Value *target, std::string index);

  std::string getDatatype(Value *addr);

  std::string createTemporaryVariable(Value *addr, std::string datatype);
  std::string getOrCreateTemporaryVariable(Value *addr);

  std::string generateVariableDeclarations();
};

class CodeGeneratorBackend {
public:
  virtual void reset() =0;

  virtual std::string generateBranchLabel(Value *target) =0;
  virtual std::string generateStore(Value *op1, Value *op2) =0;
  virtual std::string generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, std::string opcode) =0;
  virtual std::string generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args) =0;
  virtual std::string generateVoidCall(std::string funcName, std::vector<Value*> args) =0;
  virtual std::string generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate) =0;
  virtual std::string generateIntegerExtension(std::string tmpVar, Value *op) =0;
  virtual std::string generatePhiNodeAssignment(std::string tmpVar, Value *op) =0;
  virtual std::string generateUnconditionalBranch(std::string label) =0;
  virtual std::string generateConditionalBranch(Value *condition, std::string label1, std::string label2) =0;
  virtual std::string generateReturn(Value *retVal) =0;
  //virtual std::string generateVariableDeclarations() =0;
  virtual std::string generateBranchTargetIfNecessary(llvm::Instruction* instr) =0;
};

class CCodeBackend : public CodeGeneratorBackend {
  unsigned int tmpLabelNumber;
  std::map<Value*, std::vector<std::string> > branchLabels;

  SimpleCCodeGenerator* generator;
public:
  CCodeBackend(SimpleCCodeGenerator* generator);

  void reset();

  std::string generateBranchLabel(Value *target);
  std::string generateStore(Value *op1, Value *op2);
  std::string generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, std::string opcode);
  std::string generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args);
  std::string generateVoidCall(std::string funcName, std::vector<Value*> args);
  std::string generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate);
  std::string generateIntegerExtension(std::string tmpVar, Value *op);
  std::string generatePhiNodeAssignment(std::string tmpVar, Value *op);
  std::string generateUnconditionalBranch(std::string label);
  std::string generateConditionalBranch(Value *condition, std::string label1, std::string label2);
  std::string generateReturn(Value *retVal);
  //std::string generateVariableDeclarations();
  std::string generateBranchTargetIfNecessary(llvm::Instruction* instr);

private:
  std::string getOperandString(Value* addr);
};

#endif /*SIMPLE_CCODE_GENERATOR_H*/
