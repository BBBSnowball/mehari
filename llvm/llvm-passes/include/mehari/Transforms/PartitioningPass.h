#ifndef PARTITIONING_PASS_H
#define PARTITIONING_PASS_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include "mehari/Transforms/PartitioningGraph.h"

using namespace llvm;

class PartitioningPass : public FunctionPass {

public:
  static char ID;

  PartitioningPass();
  ~PartitioningPass();

  virtual bool runOnFunction(Function &func);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	enum Partitions {CPU, FPGA};
	static unsigned int partitionCount;

private:
  std::vector<std::string> targetFunctions;
  void parseTargetFunctions();
  
	void applyRandomPartitioning(PartitioningGraph &pGraph, unsigned int seed);
};

#endif /*PARTITIONING_PASS_H*/
