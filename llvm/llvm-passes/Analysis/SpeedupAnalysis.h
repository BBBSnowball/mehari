#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

using namespace llvm;

class MehariSpeedupAnalysis : public FunctionPass {

public:
  static char ID;

  MehariSpeedupAnalysis();
  ~MehariSpeedupAnalysis();

  virtual bool runOnFunction(Function &func);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

private:
	unsigned int getInstructionCost(Instruction *instruction);

};
