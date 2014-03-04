#include "mehari/Transforms/PartitioningGraph.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

#include <sstream>


PartitioningGraph::PartitioningGraph() {}
PartitioningGraph::~PartitioningGraph() {}


void PartitioningGraph::buildGraph(Function &func) {
	createVertices(func);
	addEdges();
}


void PartitioningGraph::printGraph(void) {

	Graph::vertex_iterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(pGraph); vertexIt != vertexEnd; ++vertexIt) { 
		errs() << "\n";
		errs() << "instructions in vertex: " << pGraph[*vertexIt].name << "\n";
		for (std::vector<Instruction*>::iterator instrIt = pGraph[*vertexIt].instructions.begin(); instrIt != pGraph[*vertexIt].instructions.end(); ++instrIt)
			errs() << "   " << *dyn_cast<Instruction>(*instrIt) << "\n";
	}
}


void PartitioningGraph::createVertices(Function &func) {
	// create vertices from instructions inside a function by cutting 
	// the instruction list at store instructions

	std::vector<Instruction*> worklist;
	for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
		worklist.push_back(&*I);

	std::string allocaOpcode = "alloca";
	std::string storeOpcode = "store";

	int vertexNumber = 0;
	bool init = true;
	std::vector<Instruction*> currentInstrutions;
	Graph::vertex_descriptor initVertex = boost::add_vertex(pGraph);
	pGraph[initVertex].name = "init";
	for (std::vector<Instruction*>::iterator instrIt = worklist.begin(); instrIt != worklist.end(); ++instrIt) {
		Instruction *instr = dyn_cast<Instruction>(*instrIt);
		currentInstrutions.push_back(instr);
		if (init) {
			std::string opcode = dyn_cast<Instruction>(*(instrIt+1))->getOpcodeName();
			if (opcode.compare(allocaOpcode) != 0 && opcode.compare(storeOpcode) != 0) {
				pGraph[initVertex].instructions = currentInstrutions;
				currentInstrutions.clear();
				init = false;
			}
		}
		else {
			std::string opcode = dyn_cast<Instruction>(*instrIt)->getOpcodeName();
			if (opcode.compare(storeOpcode) == 0) {
				Graph::vertex_descriptor newVertex = boost::add_vertex(pGraph);
				std::stringstream ss;
				ss << vertexNumber++;
				pGraph[newVertex].name = ss.str();
				pGraph[newVertex].instructions = currentInstrutions;
				currentInstrutions.clear();
			} 
		}
	}
}


void PartitioningGraph::addEdges() {
	// add edges between the vertices of the partitioning graph
	// that represent dependencies between the instructions inside the vertices
}
