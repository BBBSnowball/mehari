#ifndef PARTITIONING_PASS_H
#define PARTITIONING_PASS_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "mehari/Transforms/PartitioningGraph.h"
#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include <string>

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

  std::vector<SimpleCCodeGenerator::GlobalArrayVariable> globalVariables;

  void parseTargetFunctions(void);
  
	void applyRandomPartitioning(PartitioningGraph &pGraph, unsigned int seed);

  void handleDependencies(Module &M, Function &F, PartitioningGraph &pGraph, InstructionDependencyList &dependencies);

  void savePartitioning(std::map<std::string, Function*> &functions, std::map<std::string, PartitioningGraph*> &graphs);
};

#endif /*PARTITIONING_PASS_H*/