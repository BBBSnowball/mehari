#ifndef SIMPLE_CCODE_GENERATOR_H
#define SIMPLE_CCODE_GENERATOR_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

#include <string>
#include <vector>
#include <map>

using namespace llvm;

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

private:
  std::map<Value*, std::string> variables;
  std::map<Value*, std::vector<std::string> > branchLabels;
  std::map<std::string, std::vector<std::string> > tmpVariables;
  std::map<std::string, std::string> dataDependencies;
  unsigned int tmpVarNumber;
  unsigned int tmpLabelNumber;
  
  std::map<std::string, std::string> binaryOperatorStrings;
  std::map<FCmpInst::Predicate, std::string> comparePredicateStrings;  

  void resetVariables();

  void extractFunctionParameters(Function &func);

  void addVariable(Value *addr, std::string name);
  void addVariable(Value *addr, std::string name, std::string index);
  void copyVariable(Value *source, Value *target);
  void addIndexToVariable(Value *source, Value *target, std::string index);

  std::string getDatatype(Value *addr);

  std::string parseBinaryOperator(std::string opcode);
  std::string parseComparePredicate(FCmpInst::Predicate predicateNumber);

  std::string getOperandString(Value* addr);

  std::string createTemporaryVariable(Value *addr, std::string datatype);
  std::string getTemporaryVariable(Value *addr);

  std::string createBranchLabel(Value *target);

  std::string printStore(Value *op1, Value *op2);
  std::string printBinaryOperator(std::string tmpVar, Value *op1, Value *op2, std::string opcode);
  std::string printCall(std::string funcName, std::string tmpVar, std::vector<Value*> args);
  std::string printVoidCall(std::string funcName, std::vector<Value*> args);
  std::string printComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate);
  std::string printIntegerExtension(std::string tmpVar, Value *op);
  std::string printPhiNodeAssignment(std::string tmpVar, Value *op);
  std::string printUnconditionalBranch(std::string label);
  std::string printConditionalBranch(Value *condition, std::string label1, std::string label2);
  std::string printReturn(Value *retVal);

  std::string printVariableDeclarations();
};

#endif /*SIMPLE_CCODE_GENERATOR_H*/
