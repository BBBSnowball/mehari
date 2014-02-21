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

private:
	std::string createInstruction(std::vector<std::string> instructionVector);
	std::string parseOperation(std::string opcode);

	std::map<Value*, std::string> variables;
	std::map<std::string, std::string> opcodeStrings;
};