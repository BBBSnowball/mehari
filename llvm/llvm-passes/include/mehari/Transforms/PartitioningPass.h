#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

using namespace llvm;

class PartitioningPass : public FunctionPass {

public:
  static char ID;

  PartitioningPass();
  ~PartitioningPass();

  virtual bool runOnFunction(Function &func);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
};
