#include "mehari/Transforms/PartitioningGraph.h"

#include "llvm/Support/raw_ostream.h"

#include <sstream>


PartitioningGraph::PartitioningGraph() {}
PartitioningGraph::~PartitioningGraph() {}


void PartitioningGraph::createVertices(std::vector<Instruction*> &instructions) {
	// create vertices from instructions by cutting 
	// the instruction list at store instructions

	std::string allocaOpcode = "alloca";
	std::string storeOpcode = "store";

	int vertexNumber = 0;
	bool init = true;
	std::vector<Instruction*> currentInstrutions;
	Graph::vertex_descriptor initVertex = boost::add_vertex(pGraph);
	pGraph[initVertex].name = "init";
	for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {
		Instruction *instr = dyn_cast<Instruction>(*instrIt);
		currentInstrutions.push_back(instr);
		if (init) {
			std::string opcode = dyn_cast<Instruction>(*(instrIt+1))->getOpcodeName();
			if (opcode.compare(allocaOpcode) != 0 && opcode.compare(storeOpcode) != 0) {
				pGraph[initVertex].instructions = currentInstrutions;
				// save the instruction list the graph is based on
				addInstructionsToList(currentInstrutions);
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
				// save the instruction list the graph is based on
				addInstructionsToList(currentInstrutions);
				currentInstrutions.clear();
			} 
		}
	}
}


void PartitioningGraph::addEdges(InstructionDependencyList &dependencies) {
	// add edges between the vertices (ComputationUnit) of the partitioning graph
	// that represent dependencies between the instructions inside the vertices
	int index = 0;
	for (std::vector<Instruction*>::iterator instrIt = instructionList.begin(); instrIt != instructionList.end(); ++instrIt, ++index) {
		// find the ComputationUnit that contains the target instruction
		Graph::vertex_descriptor targetVertex = getVertexForInstruction(*instrIt);
		for (InstructionDependencies::iterator depIt = dependencies[index].begin(); depIt != dependencies[index].end(); ++depIt) {
			// find the ComputationUnit that contains the dependency 
			// if this ComputationUnit is not the one the target instruction is located in, create an edge between the two ComputationUnits
			Graph::vertex_descriptor dependencyVertex = getVertexForInstruction(instructionList[*depIt]);
			if (targetVertex != dependencyVertex) {
				Graph::edge_descriptor ed;
				bool inserted;
				boost::tie(ed, inserted) = boost::add_edge(dependencyVertex, targetVertex, pGraph);
				if (inserted)
					// initialize the communication cost
					pGraph[ed].cost = 1;
				else
					// there already is an edge between the two vertices -> increment the communication cost
					pGraph[ed].cost += 1;
			}	
		}
	}
}


PartitioningGraph::VertexDescriptor PartitioningGraph::getVertexForInstruction(Instruction *instruction) {
	Graph::vertex_iterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(pGraph); vertexIt != vertexEnd; ++vertexIt) {
		std::vector<Instruction*> instrList = pGraph[*vertexIt].instructions;
		if (std::find(instrList.begin(), instrList.end(), instruction) != instrList.end())
			return *vertexIt;
	}
}


void PartitioningGraph::addInstructionsToList(std::vector<Instruction*> instructions) {
	for (std::vector<Instruction*>::iterator it = instructions.begin(); it != instructions.end(); ++it)
		instructionList.push_back(*it);
}


PartitioningGraph::VertexIterator PartitioningGraph::getFirstIterator() {
	VertexIterator vIt, vEnd;
	boost::tie(vIt, vEnd) = boost::vertices(pGraph);
	return vIt;
}

PartitioningGraph::VertexIterator PartitioningGraph::getEndIterator() {
	VertexIterator vIt, vEnd;
	boost::tie(vIt, vEnd) = boost::vertices(pGraph);
	return vEnd;
}


void PartitioningGraph::setPartition(PartitioningGraph::VertexDescriptor vd, unsigned int partition) {
	pGraph[vd].partition = partition;	
}


void PartitioningGraph::printGraph(std::string &name) {
	errs() << "Partitioning Graph: " << name << "\n";
	errs() << "\nVERTICES:\n";
	Graph::vertex_iterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(pGraph); vertexIt != vertexEnd; ++vertexIt) { 
		errs() << "\n";
		errs() << "vertex " << pGraph[*vertexIt].name << ":\n";
		errs() << "-------\n";
		errs() << "partition: " << pGraph[*vertexIt].partition << "\n";
		errs() << "instructions in vertex:\n";
		for (std::vector<Instruction*>::iterator instrIt = pGraph[*vertexIt].instructions.begin(); instrIt != pGraph[*vertexIt].instructions.end(); ++instrIt)
			errs() << "   " << *dyn_cast<Instruction>(*instrIt) << "\n";
	}

	errs() << "\nEDGES:\n";
	Graph::edge_iterator edgeIt, edgeEnd;
	for (boost::tie(edgeIt, edgeEnd) = boost::edges(pGraph); edgeIt != edgeEnd; ++edgeIt) {
    Graph::vertex_descriptor u = boost::source(*edgeIt, pGraph), v = boost::target(*edgeIt, pGraph);
		errs() << pGraph[u].name << " -> " << pGraph[v].name << " (cost: " << pGraph[*edgeIt].cost << ")\n";
	}
}


void PartitioningGraph::printGraphviz(std::string &name) {
	// set some node colors to visualize the different partitions
	const char *nodeColors[] = { "greenyellow", "gold", "cornflowerblue", "darkorange", "aquamarine1" };

	std::string filename = "_output/graph/partitioning-graph-" + name + ".dot";
  std::ofstream dotfile(filename.c_str());

  dotfile << "digraph G {\n"
    << "  edge[style=\"bold\"];\n" 
    << "  node[shape=\"rectangle\"];\n";

  Graph::vertex_iterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(pGraph); vertexIt != vertexEnd; ++vertexIt) {
		dotfile << "  " << pGraph[*vertexIt].name << "[style=filled, fillcolor=" << nodeColors[pGraph[*vertexIt].partition] << ", label=\"" << pGraph[*vertexIt].name << ":\\n\\n\"\n";
		std::vector<Instruction*> instructions = pGraph[*vertexIt].instructions;
		for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {
			std::string instrString;
			Instruction *instr = dyn_cast<Instruction>(*instrIt);
    	llvm::raw_string_ostream rso(instrString);
    	instr->print(rso);
    	dotfile << "  + \"" << instrString << "\\l\"\n";
  	}
		dotfile << "  ];\n";
	}

	Graph::edge_iterator edgeIt, edgeEnd;
	for (boost::tie(edgeIt, edgeEnd) = boost::edges(pGraph); edgeIt != edgeEnd; ++edgeIt) {
    Graph::vertex_descriptor u = boost::source(*edgeIt, pGraph), v = boost::target(*edgeIt, pGraph);
		dotfile << "  " << pGraph[u].name << "->" << pGraph[v].name << "[label=\"" << pGraph[*edgeIt].cost << "\"];\n";
	}

  dotfile << "}";
}
