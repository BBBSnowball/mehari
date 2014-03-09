#include "mehari/Transforms/PartitioningPass.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"


unsigned int PartitioningPass::partitionCount = 2;


PartitioningPass::PartitioningPass() : FunctionPass(ID) {}
PartitioningPass::~PartitioningPass() {}


bool PartitioningPass::runOnFunction(Function &func) {
	// create worklist containing all instructions of the function
	std::vector<Instruction*> worklist;
  	for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    	worklist.push_back(&*I);

  // run InstructionDependencyAnalysis
  InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>();
  InstructionDependencyList dependencies = IDA->getDependencies(worklist);

  // create partitioning graph
	PartitioningGraph pGraph;
	pGraph.createVertices(worklist);
	pGraph.addEdges(dependencies);

	// create partitioning
	applyRandomPartitioning(pGraph, 42);

	// print results
	pGraph.printGraph();
	pGraph.printGraphviz();

	return false;
}


void PartitioningPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredTransitive<InstructionDependencyAnalysis>();
  AU.setPreservesAll();
}


void PartitioningPass::applyRandomPartitioning(PartitioningGraph &pGraph, unsigned int seed) {
	srand(seed);
	PartitioningGraph::VertexIterator vIt = pGraph.getFirstIterator(); 
	PartitioningGraph::VertexIterator vEnd = pGraph.getEndIterator();
	for (; vIt != vEnd; ++vIt) {
		PartitioningGraph::VertexDescriptor vd = *vIt;
		pGraph.setPartition(vd, rand()%partitionCount);
	}
}


// register pass so we can call it using opt
char PartitioningPass::ID = 0;
static RegisterPass<PartitioningPass>
Y("partitioning", "Create a partitioning for the calculation inside the function.");
