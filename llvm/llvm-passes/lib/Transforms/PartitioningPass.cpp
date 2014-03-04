#include "mehari/Transforms/PartitioningPass.h"
#include "mehari/Transforms/PartitioningGraph.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"


PartitioningPass::PartitioningPass() : FunctionPass(ID) {}
PartitioningPass::~PartitioningPass() {}


bool PartitioningPass::runOnFunction(Function &func) {
	PartitioningGraph pGraph;
	pGraph.buildGraph(func);
	pGraph.printGraph();

	return false;
}


void PartitioningPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}


// register pass so we can call it using opt
char PartitioningPass::ID = 0;
static RegisterPass<PartitioningPass>
Y("partitioning", "Create a partitioning for the calculation inside the function.");
