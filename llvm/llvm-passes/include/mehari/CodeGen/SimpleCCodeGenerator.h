#ifndef SIMPLE_CCODE_GENERATOR_H
#define SIMPLE_CCODE_GENERATOR_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

using namespace llvm;

class SimpleCCodeGenerator : public FunctionPass {

public:
  static char ID;

  SimpleCCodeGenerator();
  ~SimpleCCodeGenerator();

  virtual bool runOnFunction(Function &func);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  std::string createCCode(std::vector<Instruction*> &instructions);

private:
  std::map<Value*, std::string> variables;
  std::map<Value*, std::vector<std::string> > branchLabels;
  std::map<std::string, std::vector<std::string> > tmpVariables;
  unsigned int tmpVarNumber;
  unsigned int tmpLabelNumber;
  
  std::map<std::string, std::string> binaryOperatorStrings;
  std::map<unsigned int, std::string> comparePredicateStrings;  

  void resetVariables();

  void addVariable(Value *addr, std::string name);
  void addVariable(Value *addr, std::string name, std::string index);
  void copyVariable(Value *source, Value *target);
  void addIndexToVariable(Value *source, Value *target, std::string index);

  std::string parseBinaryOperator(std::string opcode);
  std::string parseComparePredicate(unsigned int predicateNumber);

  std::string getOperandString(Value* addr);

  std::string createTemporaryVariable(Value *addr, std::string datatype);
  std::string getTemporaryVariable(Value *addr);

  std::string createBranchLabel(Value *target);

  std::string printStore(Value *op1, Value *op2);
  std::string printBinaryOperator(std::string tmpVar, Value *op1, Value *op2, std::string opcode);
  std::string printCall(std::string tmpVar, Value *arg, std::string funcName);
  std::string printComparison(std::string tmpVar, Value *op1, Value *op2, unsigned int comparePredicate);
  std::string printIntegerExtension(std::string tmpVar, Value *op);
  std::string printPhiNodeAssignment(std::string tmpVar, Value *op);
  std::string printUnconditionalBranch(std::string label);
  std::string printConditionalBranch(Value *condition, std::string label1, std::string label2);
  std::string printReturn(Value *retVal);

  std::string printVariableDeclarations();
};

#endif /*SIMPLE_CCODE_GENERATOR_H*/
