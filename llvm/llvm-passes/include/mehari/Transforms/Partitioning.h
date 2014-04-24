#ifndef PARTITIONING_PASS_H
#define PARTITIONING_PASS_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "mehari/Transforms/PartitioningGraph.h"

using namespace llvm;

class Partitioning : public ModulePass {

public:
  static char ID;

  Partitioning();
  ~Partitioning();

  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	enum Architectures {CPU, FPGA};

private:
  std::vector<std::string> targetFunctions;
  unsigned int partitionCount;

  void parseTargetFunctions();
  
	void applyRandomPartitioning(PartitioningGraph &pGraph, unsigned int seed);

  void writePartitioning();
};

#endif /*PARTITIONING_PASS_H*/
