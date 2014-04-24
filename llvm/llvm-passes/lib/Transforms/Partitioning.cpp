#include "mehari/Transforms/Partitioning.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"
#include "mehari/CodeGen/SimpleCCodeGenerator.h"
#include "mehari/CodeGen/TemplateWriter.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include <sstream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>


static cl::opt<std::string> TargetFunctions("partitioning-functions", 
            cl::desc("Specify the functions the partitioning will be applyed on (seperated by whitespace)"), 
            cl::value_desc("target-functions"));
static cl::opt<std::string> PartitionCount("partitioning-count", 
            cl::desc("Specify the number ob partitions"), 
            cl::value_desc("partitioning-count"));
static cl::opt<std::string> OutputDir("partitioning-output-dir", 
            cl::desc("Set the output directory for graph results"), 
            cl::value_desc("partitioning-output-dir"));
static cl::opt<std::string> TemplateDir("template-dir", 
            cl::desc("Set the directory where the code generation templates are located"), 
            cl::value_desc("template-dir"));


Partitioning::Partitioning() : ModulePass(ID) {
	initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
	// read command line arguments
	parseTargetFunctions();
	std::stringstream ss(PartitionCount);
	ss >> partitionCount;
}

Partitioning::~Partitioning() {}


bool Partitioning::runOnModule(Module &M) {

	// create partitioning for each target function
	for (std::vector<std::string>::iterator funcIt = targetFunctions.begin(); funcIt != targetFunctions.end(); ++funcIt) {
		Function *func = M.getFunction(*funcIt);
		std::string functionName = func->getName().str();

		errs() << "\npartitioning: " << functionName << "\n\n";

		// create worklist containing all instructions of the function
		std::vector<Instruction*> worklist;
		for (inst_iterator I = inst_begin(*func), E = inst_end(*func); I != E; ++I)
			worklist.push_back(&*I);

		// run InstructionDependencyAnalysis
		InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>(*func);
		InstructionDependencyList dependencies = IDA->getDependencies(*func);

		// create partitioning graph
		PartitioningGraph pGraph;
		pGraph.createVertices(worklist);
		pGraph.addEdges(dependencies);
		
		// create partitioning
		applyRandomPartitioning(pGraph, 42);
		
		// print partitioning graph results
		pGraph.printGraphviz(functionName, OutputDir);
	}

	// print partitioning results into template
	writePartitioning();

	return false;
}


void Partitioning::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<InstructionDependencyAnalysis>();
	AU.setPreservesAll();
}


void Partitioning::parseTargetFunctions() {
  boost::algorithm::split(targetFunctions, TargetFunctions, boost::algorithm::is_any_of(" "));
}


void Partitioning::applyRandomPartitioning(PartitioningGraph &pGraph, unsigned int seed) {
	srand(seed);
	PartitioningGraph::VertexIterator vIt = pGraph.getFirstIterator(); 
	PartitioningGraph::VertexIterator vEnd = pGraph.getEndIterator();
	for (; vIt != vEnd; ++vIt) {
		PartitioningGraph::VertexDescriptor vd = *vIt;
		pGraph.setPartition(vd, rand()%partitionCount);
	}
}


void Partitioning::writePartitioning() {
	std::string mehariTemplate = TemplateDir + "/mehari.tpl";
	std::string outputFile = OutputDir + "/mehari.c";
	TemplateWriter tWriter;

	// TODO: set values depending on the partitioning results
	tWriter.setValue("SEMAPHORE_COUNT", "5");
	tWriter.setValue("DATA_DEP_COUNT", "8");
	tWriter.setValue("THREAD_COUNT", "1");

	tWriter.expandTemplate(mehariTemplate);
	tWriter.writeToFile(outputFile);
}

// void Partitioning::writePartitioningToFile(Function &func, PartitioningGraph &pGraph) {
// 	// collect the instructions for each partition
// 	std::vector<Instruction*> instructionsForPartition[partitionCount];
// 	PartitioningGraph::VertexIterator vIt = pGraph.getFirstIterator(); 
// 	PartitioningGraph::VertexIterator vEnd = pGraph.getEndIterator();
// 	for (; vIt != vEnd; ++vIt) {
// 		PartitioningGraph::VertexDescriptor vd = *vIt;
// 		unsigned int partitionNumber = pGraph.getPartition(vd);
// 		std::vector<Instruction*> instructions = pGraph.getInstructions(vd);
// 		for (std::vector<Instruction*>::iterator it = instructions.begin(); it != instructions.end(); ++it) {
// 			Instruction *instr = dyn_cast<Instruction>(*it);
// 			instructionsForPartition[partitionNumber].push_back(instr);
// 		}
// 	}
// 	// generate C code for each partition and print it to file
// 	std::string filename = "_output/partitioning-result.c";
// 	std::ofstream cfile(filename.c_str());
// 	SimpleCCodeGenerator codeGen;
// 	for (int i=0; i<partitionCount; i++) {
// 		cfile << "\n\n-----------\n" << "### " << i << ":\n" << "-----------\n\n";
// 		cfile << codeGen.createCCode(func, instructionsForPartition[i]);
// 	}
// }


// register pass so we can call it using opt
char Partitioning::ID = 0;
static RegisterPass<Partitioning>
Y("partitioning", "Create a partitioning for the calculation inside the function.");
