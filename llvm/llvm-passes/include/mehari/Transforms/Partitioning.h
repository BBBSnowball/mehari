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

  std::vector<std::string> dataDependencies;
  unsigned int depNumber;
  unsigned int semNumberMax;

  std::vector<SimpleCCodeGenerator::GlobalArrayVariable> globalVariables;

  void parseTargetFunctions(void);

  void handleDependencies(Module &M, Function &F, PartitioningGraph &pGraph, InstructionDependencyList &dependencies);

  void savePartitioning(std::map<std::string, Function*> &functions, std::map<std::string, PartitioningGraph*> &graphs, 
    std::map<std::string, unsigned int> partitioningNumbers);
};

#endif /*PARTITIONING_PASS_H*/
