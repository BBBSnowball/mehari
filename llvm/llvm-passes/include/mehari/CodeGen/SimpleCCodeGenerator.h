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
  std::map<Value*, std::string> phiNodeOperands;
  std::map<Value*, std::vector<std::string> > branchLabels;
  
  std::map<std::string, std::string> binaryOperatorStrings;
  std::map<unsigned int, std::string> comparePredicateStrings;  

  std::string parseBinaryOperator(std::string opcode);
  std::string parseComparePredicate(unsigned int predicateNumber);
  std::string getOperandString(Value* addr);
};

#endif /*SIMPLE_CCODE_GENERATOR_H*/
