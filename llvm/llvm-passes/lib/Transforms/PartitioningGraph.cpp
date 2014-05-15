#include "mehari/Transforms/PartitioningGraph.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include <sstream>


PartitioningGraph::PartitioningGraph() {}
PartitioningGraph::~PartitioningGraph() {}


void PartitioningGraph::create(std::vector<Instruction*> &instructions, InstructionDependencyNumbersList &dependencies) {
	createVertices(instructions);
	addEdges(dependencies);

	// remove init vertex because it only handles parameter initialization that is not 
	// useful for the partitioning and C code generation
	boost::remove_vertex(initVertex, pGraph);
}


void PartitioningGraph::createVertices(std::vector<Instruction*> &instructions) {
	// create vertices from instructions by cutting 
	// the instruction list at specific instructions
	// if statements and pointer assignment are kept together

	// detemine number of parameters
    unsigned int paramCount = 0;
    for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt)
      if (isa<AllocaInst>(*instrIt))
        paramCount++;

	int vertexNumber = 0;
	int instrNumber = 0;
	bool isBlock = false;
	bool isBlockCompleted = false;
	bool isPtrAssignment = false;
	initVertex = boost::add_vertex(pGraph);
	pGraph[initVertex].name = "init";
	std::vector<Instruction*> currentInstrutions;
	for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {
		Instruction *instr = dyn_cast<Instruction>(*instrIt);
		Instruction *nextInstr;
		if (instrIt+1 != instructions.end())
      		nextInstr = dyn_cast<Instruction>(*(instrIt+1));
		currentInstrutions.push_back(instr);
		// handle init vertex
		if (instrNumber == 2*paramCount-1) {
				pGraph[initVertex].instructions = currentInstrutions;
				// save the instruction list the graph is based on
				addInstructionsToList(currentInstrutions);
				currentInstrutions.clear();
		}
		// handle the calculations
		else if (instrNumber >= 2*paramCount) {
			// do we currently handle an if statement?
			if (isa<BranchInst>(instr)) {
				if (!isBlock)
					isBlock = true;
				else
					isBlockCompleted = true;
			}
			else if (isa<LoadInst>(instr) && instr->getOperand(0)->getName() == "status.addr")
				isPtrAssignment = true;
			else if (isPtrAssignment && isa<StoreInst>(instr))
				isPtrAssignment = false;

			if (isCuttingInstr(instr)) {
				if (!isPtrAssignment && (!isBlock || (isBlock && isBlockCompleted))) {
					Graph::vertex_descriptor newVertex = boost::add_vertex(pGraph);
					std::stringstream ss;
					ss << vertexNumber++;
					pGraph[newVertex].name = ss.str();
					pGraph[newVertex].instructions = currentInstrutions;
					addInstructionsToList(currentInstrutions);
					currentInstrutions.clear();
					isBlock = false;
					isBlockCompleted = false;
				}
			}
		} // end handle calculations
		instrNumber++;
	}
}

bool PartitioningGraph::isCuttingInstr(Instruction *instr) {
	return (isa<StoreInst>(instr)
		||	isa<BinaryOperator>(instr)
		||	isa<CallInst>(instr)
		||	isa<CmpInst>(instr)
		||	isa<ZExtInst>(instr)
		||	isa<ReturnInst>(instr));
}

void PartitioningGraph::addEdges(InstructionDependencyNumbersList &dependencies) {
	// add edges between the vertices (ComputationUnit) of the partitioning graph
	// that represent dependencies between the instructions inside the vertices
	int index = 0;
	for (std::vector<Instruction*>::iterator instrIt = instructionList.begin(); instrIt != instructionList.end(); ++instrIt, ++index) {
		// find the ComputationUnit that contains the target instruction
		Graph::vertex_descriptor targetVertex = getVertexForInstruction(*instrIt);
		if (targetVertex == NULL)
			// the target instruction is not part of the Graph -> continue with the next instruction
			continue;
		for (InstructionDependencyNumbers::iterator depIt = dependencies[index].begin(); depIt != dependencies[index].end(); ++depIt) {
			// find the ComputationUnit that contains the dependency 
			// if this ComputationUnit is not the one the target instruction is located in, create an edge between the two ComputationUnits
			Graph::vertex_descriptor dependencyVertex = getVertexForInstruction(instructionList[*depIt]);
			if (dependencyVertex == NULL)
				// the dependency instruction is not part of the Graph -> continue with the next instruction
				continue;
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
	return NULL;
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


unsigned int PartitioningGraph::getPartition(VertexDescriptor vd) {
	return pGraph[vd].partition;
}


void PartitioningGraph::setInstructions(VertexDescriptor vd, std::vector<Instruction*> &instructions) {
	pGraph[vd].instructions = instructions;
}


std::vector<Instruction*> &PartitioningGraph::getInstructions(VertexDescriptor vd) {
	return pGraph[vd].instructions;
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


void PartitioningGraph::printGraphviz(Function &func, std::string &name, std::string &outputDir) {
	// set some node colors to visualize the different partitions
	const char *nodeColors[] = { "greenyellow", "gold", "cornflowerblue", "darkorange", "aquamarine1" };

	std::string filename = outputDir + "/partitioning-graph-" + name + ".dot";
	std::ofstream dotfile(filename.c_str());

	SimpleCCodeGenerator codegen;

	dotfile << "digraph G {\n"
		<< "  edge[style=\"bold\"];\n" 
		<< "  node[shape=\"rectangle\"];\n";

	Graph::vertex_iterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(pGraph); vertexIt != vertexEnd; ++vertexIt) {
		dotfile << "  " << pGraph[*vertexIt].name << "[style=filled, fillcolor=" << nodeColors[pGraph[*vertexIt].partition] << ", label=\"" << pGraph[*vertexIt].name << ":\\n\\n\"\n";
		std::vector<Instruction*> instructions = pGraph[*vertexIt].instructions;
		if (false) {
			for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {
				std::string instrString;
				Instruction *instr = dyn_cast<Instruction>(*instrIt);
				llvm::raw_string_ostream rso(instrString);
				instr->print(rso);
				dotfile << "  + \"" << instrString << "\\l\"\n";
			}
			dotfile << "  + \"\\n\"\n";
		}
		std::istringstream f(codegen.createCCode(func, instructions));
		std::string line;  
		while (std::getline(f, line))
			dotfile << "  + \"" << line << "\\l\"\n";
		dotfile << "  ];\n";
	}

	Graph::edge_iterator edgeIt, edgeEnd;
	for (boost::tie(edgeIt, edgeEnd) = boost::edges(pGraph); edgeIt != edgeEnd; ++edgeIt) {
    Graph::vertex_descriptor u = boost::source(*edgeIt, pGraph), v = boost::target(*edgeIt, pGraph);
		dotfile << "  " << pGraph[u].name << "->" << pGraph[v].name << "[label=\"" << pGraph[*edgeIt].cost << "\"];\n";
	}

  dotfile << "}";
}
