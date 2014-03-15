#include "mehari/Transforms/PartitioningPass.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/graphviz.hpp>


// TODO set the partition count via command line parameter
unsigned int PartitioningPass::partitionCount = 2;


static cl::opt<std::string> TargetFunctions("partitioning-functions", 
            cl::desc("Specify the functions the partitioning will be applyed on (seperated by whitespace)"), 
            cl::value_desc("target-functions"));


PartitioningPass::PartitioningPass() : FunctionPass(ID) {
  parseTargetFunctions();
}

PartitioningPass::~PartitioningPass() {}


bool PartitioningPass::runOnFunction(Function &func) {

  // just handle those functions specified by the command line parameter
  if (std::find(targetFunctions.begin(), targetFunctions.end(), func.getName()) == targetFunctions.end())
    return false;
  
  errs() << "\n\npartitioning: " << func.getName() << "\n";

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


void PartitioningPass::parseTargetFunctions() {
  boost::algorithm::split(targetFunctions, TargetFunctions, boost::algorithm::is_any_of(" "));
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
