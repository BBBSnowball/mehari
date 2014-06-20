#include "mehari/Transforms/PartitioningGraph.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "mehari/HardwareInformation.h"
#include "mehari/CodeGen/SimpleCCodeGenerator.h"

// user RandomGenerator and random_vertex
#include <boost/graph/random.hpp>
#include <boost/random.hpp>

#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>

#include <ctime> // for using random generator with time
#include <sstream>
#include <set>


PartitioningGraph::PartitioningGraph() {}
PartitioningGraph::~PartitioningGraph() {}


// overwrite copy constructor and assignment operator to create a deep copy of PartitioningGraphs
PartitioningGraph::PartitioningGraph(const PartitioningGraph &cSource) {
	instructionList = cSource.instructionList;
	copyGraph(cSource.pGraph, pGraph);
	initVertex = cSource.initVertex;
}

PartitioningGraph &PartitioningGraph::operator=(const PartitioningGraph &cSource) {
	// check for self-assignmend
	if (this == &cSource)
		return *this;
	
	// now copy the class members
	instructionList = cSource.instructionList;
	copyGraph(cSource.pGraph, pGraph);
	initVertex = cSource.initVertex;

	return *this;
}


void PartitioningGraph::create(std::vector<Instruction*> &instructions, InstructionDependencyList &dependencies) {
	createVertices(instructions);
	addEdges(dependencies);

	// remove init vertex because it only handles parameter initialization that is not 
	// useful for the partitioning and C code generation
	boost::clear_vertex(initVertex, pGraph);
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
	unsigned int instrNumber = 0;
	bool isBlock = false;
	bool isBlockCompleted = false;
	bool isPtrAssignment = false;
	initVertex = boost::add_vertex(pGraph);
	pGraph[initVertex].name = "init";
	pGraph[initVertex].partition = -1;
	std::vector<Instruction*> currentInstrutions;
	for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {
		Instruction *instr = dyn_cast<Instruction>(*instrIt);
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
			// if statement detection: 
			//   - IF-Start: conditional branch instruction
			//   - IF-End:   unconditional branch instruction and next BB ends with no unconditional branch instruction 
			if (BranchInst *bInstr = dyn_cast<BranchInst>(instr)) {
				if (!isBlock) {
					isBlock = true;
				}
				else {
					Instruction *terminatorInstrNextBB = dyn_cast<TerminatorInst>(bInstr)->getSuccessor(0)->getTerminator();
					if (bInstr->isUnconditional() &&
						(!isa<BranchInst>(terminatorInstrNextBB)
						|| (isa<BranchInst>(terminatorInstrNextBB)
							&& !dyn_cast<BranchInst>(terminatorInstrNextBB)->isUnconditional()))) {
						isBlockCompleted = true;
					}
				}
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
					pGraph[newVertex].partition = 0;
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

void PartitioningGraph::addEdges(InstructionDependencyList &dependencies) {
	// add edges between the vertices (ComputationUnit) of the partitioning graph
	// that represent dependencies between the instructions inside the vertices
	int index = 0;
	for (std::vector<Instruction*>::iterator instrIt = instructionList.begin(); instrIt != instructionList.end(); ++instrIt, ++index) {
		// find the ComputationUnit that contains the target instruction
		Graph::vertex_descriptor targetVertex = getVertexForInstruction(*instrIt);
		if (targetVertex == NO_SUCH_VERTEX)
			// the target instruction is not part of the Graph -> continue with the next instruction
			continue;
		for (std::vector<InstructionDependency>::iterator depIt = dependencies[index].dependencies.begin(); depIt != dependencies[index].dependencies.end(); ++depIt) {
			// find the ComputationUnit that contains the dependency 
			// if this ComputationUnit is not the one the target instruction is located in, create an edge between the two ComputationUnits
			Graph::vertex_descriptor dependencyVertex = getVertexForInstruction(depIt->depInstruction);
			if (dependencyVertex == NO_SUCH_VERTEX)
				// the dependency instruction is not part of the Graph -> continue with the next instruction
				continue;
			if (targetVertex != dependencyVertex) {
				Graph::edge_descriptor ed;
				bool inserted;
				boost::tie(ed, inserted) = boost::add_edge(dependencyVertex, targetVertex, pGraph);
				CommunicationType newCommunication;
				// TODO: find appropriate values for data and memory dependency costs
				if (depIt->isRegdep) // register depdendency -> use of a data dependency method (e.g. mbox)
					newCommunication = DataDependency;
				else // memory or control dependency -> use of a semaphore
					newCommunication = OrderDependency;
				pGraph[ed].comOperations.push_back(newCommunication);
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
	return NO_SUCH_VERTEX;
}


void PartitioningGraph::addInstructionsToList(std::vector<Instruction*> instructions) {
	for (std::vector<Instruction*>::iterator it = instructions.begin(); it != instructions.end(); ++it)
		instructionList.push_back(*it);
}


void PartitioningGraph::copyGraph(const Graph &orig, Graph &copy) {
	typedef std::map<VertexDescriptor, size_t> IndexMap;
	IndexMap mapIndex;
	boost::associative_property_map<IndexMap> propmapIndex(mapIndex);
	int i=0;
	BGL_FORALL_VERTICES(v, orig, Graph) {
		boost::put(propmapIndex, v, i++);
	}
	copy.clear();
	boost::copy_graph(orig, copy, boost::vertex_index_map(propmapIndex));
}


unsigned int PartitioningGraph::getVertexCount(void) {
	return boost::num_vertices(pGraph);
}


PartitioningGraph::VertexDescriptor PartitioningGraph::getRandomVertex(void) {
	boost::mt19937 gen(time(0));
	return boost::random_vertex(pGraph, gen);
}


PartitioningGraph::VertexIterator PartitioningGraph::getFirstIterator() {
	return boost::vertices(pGraph).first;
}

PartitioningGraph::VertexIterator PartitioningGraph::getEndIterator() {
	return boost::vertices(pGraph).second;
}


PartitioningGraph::EdgeIterator PartitioningGraph::getFirstEdgeIterator() {
	return boost::edges(pGraph).first;
}

PartitioningGraph::EdgeIterator PartitioningGraph::getEndEdgeIterator() {
	return boost::edges(pGraph).second;
}


PartitioningGraph::VertexDescriptor PartitioningGraph::getSourceVertex(EdgeDescriptor ed) {
	return boost::source(ed, pGraph);
}

PartitioningGraph::VertexDescriptor PartitioningGraph::getTargetVertex(EdgeDescriptor ed) {
	return boost::target(ed, pGraph);
}


std::string PartitioningGraph::getName(VertexDescriptor vd) {
	return pGraph[vd].name;
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


unsigned int PartitioningGraph::calcEdgeCost(EdgeDescriptor ed, 
		std::string &sourceDevice, std::string &targetDevice) {
	unsigned int costs = 0;
	HardwareInformation hwInfo;
  	DeviceInformation *devInfo = hwInfo.getDeviceInfo(sourceDevice);
  	CommunicationInformation *comInfo = devInfo->getCommunicationInfo(targetDevice);
  	for (std::vector<CommunicationType>::iterator it = pGraph[ed].comOperations.begin(); 
		it != pGraph[ed].comOperations.end(); ++it) 
		costs += comInfo->getCommunicationCost(*it);
	return costs;
}


unsigned int PartitioningGraph::calcDeviceIndependentEdgeCost(EdgeDescriptor ed) {
	unsigned int costs = 0;
	HardwareInformation hwInfo;
  	for (std::vector<CommunicationType>::iterator it = pGraph[ed].comOperations.begin(); 
		it != pGraph[ed].comOperations.end(); ++it) 
		costs += hwInfo.getDeviceIndependentCommunicationCost(*it);
	return costs;
}


unsigned int PartitioningGraph::getCommunicationCost(VertexDescriptor vd1, VertexDescriptor vd2, 
		std::string &sourceDevice, std::string &targetDevice) {
	bool exists1, exists2;
	EdgeDescriptor ed1, ed2;
	boost::tie(ed1, exists1) = boost::edge(vd1, vd2, pGraph);
	boost::tie(ed2, exists2) = boost::edge(vd2, vd1, pGraph);
	unsigned int costs = 0;
	if (exists1)
		costs += calcEdgeCost(ed1, sourceDevice, targetDevice);
	if (exists2)
		costs += calcEdgeCost(ed2, sourceDevice, targetDevice);
	return costs;
}


unsigned int PartitioningGraph::getDeviceIndependentCommunicationCost(VertexDescriptor vd1, VertexDescriptor vd2) {
	bool exists1, exists2;
	EdgeDescriptor ed1, ed2;
	boost::tie(ed1, exists1) = boost::edge(vd1, vd2, pGraph);
	boost::tie(ed2, exists2) = boost::edge(vd2, vd1, pGraph);
	unsigned int costs = 0;
	if (exists1)
		costs += calcDeviceIndependentEdgeCost(ed1);
	if (exists2)
		costs += calcDeviceIndependentEdgeCost(ed2);
	return costs;
}


unsigned int PartitioningGraph::getExecutionTime(VertexDescriptor vd, std::string &targetDevice) {
	// TODO: implement this function by using the HardwareInformation class
	HardwareInformation hwInfo;
	DeviceInformation *devInfo = hwInfo.getDeviceInfo(targetDevice);
	unsigned int texe = 0;
	std::vector<Instruction*> instrList = getInstructions(vd);
	for (std::vector<Instruction*>::iterator it = instrList.begin(); it != instrList.end(); ++it) {
		InstructionInformation *instrInfo = devInfo->getInstructionInfo((*it)->getOpcodeName());
		instrInfo != NULL ? texe += instrInfo->getCycleCount() : texe += 1;		
	}
	return texe;
}


boost::tuple<unsigned int, unsigned int> PartitioningGraph::getInternalExternalCommunicationCost(
	VertexDescriptor vd, std::vector<std::string> &partitioningDevices) {
	Graph::out_edge_iterator oeIt, oeEnd;
	Graph::in_edge_iterator ieIt, ieEnd;
	boost::tie(oeIt, oeEnd) = boost::out_edges(vd, pGraph);
	boost::tie(ieIt, ieEnd) = boost::in_edges(vd, pGraph);
	unsigned int intCosts = 0, extCosts = 0;
	for (; oeIt != oeEnd; ++oeIt) {
		VertexDescriptor u = boost::source(*oeIt, pGraph), v = boost::target(*oeIt, pGraph);
		if (pGraph[u].partition == pGraph[v].partition)
			intCosts += calcEdgeCost(*oeIt, partitioningDevices[pGraph[u].partition], 
				partitioningDevices[pGraph[v].partition]);
		else
			extCosts += calcEdgeCost(*oeIt, partitioningDevices[pGraph[u].partition], 
				partitioningDevices[pGraph[v].partition]);
	}
	for (; ieIt != ieEnd; ++ieIt) {
		VertexDescriptor u = boost::source(*ieIt, pGraph), v = boost::target(*ieIt, pGraph);
		if (pGraph[u].partition == pGraph[v].partition)
			intCosts += calcEdgeCost(*ieIt, partitioningDevices[pGraph[u].partition], 
				partitioningDevices[pGraph[v].partition]);
		else
			extCosts += calcEdgeCost(*ieIt, partitioningDevices[pGraph[u].partition], 
				partitioningDevices[pGraph[v].partition]);;
	}
	return boost::make_tuple(intCosts, extCosts);
}


unsigned int PartitioningGraph::getCriticalPathCost(std::vector<std::string> &partitioningDevices) {
	// create new simple graph type for critical path analysis
	typedef boost::adjacency_list<
		boost::setS, 				// store out-edges of each vertex in a set to avoid parallel edges
		boost::vecS, 				// store vertex set in a std::vector
		boost::directedS, 			// the graph has got directed edges
		boost::no_property,			// the vertices have no property
		boost::property<boost::edge_weight_t, int>	// the edges weight is of integer type
		> cpGraph;

	// new graph
	cpGraph tmpGraph;

	// copy vertices to new graph
	for (unsigned int i=0; i<boost::num_vertices(pGraph); i++)
		boost::add_vertex(tmpGraph);

	// create edges and set edge weight of new Graph:
	// only keep communication cost if the two vertices of the are in different partitions
	// add execution time of the target vertex to each edge weight
	EdgeIterator eIt, eEnd;
	boost::tie(eIt, eEnd) = boost::edges(pGraph);
	for( ; eIt != eEnd; ++eIt) {
		// calculate edge weight
		int edgeWeight = 0;
		VertexDescriptor u = boost::source(*eIt, pGraph), v = boost::target(*eIt, pGraph);
		if (pGraph[u].partition != pGraph[v].partition)
			edgeWeight += calcEdgeCost(*eIt, partitioningDevices[pGraph[u].partition], partitioningDevices[pGraph[v].partition]);
		edgeWeight += getExecutionTime(v, partitioningDevices[pGraph[v].partition]);
		edgeWeight *= (-1);
		// create new edge and set edge weight in temporary graph
		boost::add_edge(u, v, edgeWeight, tmpGraph);
	}

	// if we calculate the critical path for a processor execution, we need to schedule the operations
	// if we use a FPGA we can run everything in parallel, so we do not need a scheduling
	// we can create a schedule for a partition that is executed on the processor
	// by connecting every consecutive vertices (determined by vertex number) of the same partition

	// TODO: use a parameter to get the partitions and also the device that is used by the partition
	// 		 to only apply this modification for processor partitions
	// collect the used partitions 
	if (true) {
		std::set<unsigned int> partitions;
		for (unsigned int i=0; i<boost::num_vertices(pGraph); i++)
			partitions.insert(pGraph[i].partition);
		// create a schedule for each partition
		for (std::set<unsigned int>::iterator it = partitions.begin(); it != partitions.end(); ++it) {
			int currentVertex = (-1);
			for (int i=boost::num_vertices(pGraph)-1; i>=0; i--) {
				// check the current partition
				if (pGraph[i].partition == *it) {
					if (currentVertex >= 0) {
						// add an edge with the edge cost of the execution time of the target vertex 
						// if there is no edge yet
						bool exists = boost::edge(i, currentVertex, tmpGraph).second;
						if (!exists) 
							boost::add_edge(i, currentVertex, (-1)*getExecutionTime(currentVertex, 
								partitioningDevices[pGraph[currentVertex].partition]), tmpGraph);
					}
					currentVertex = i;
				}
			}
		}
	}

	// run shortest path algorithm to determine critical path (because of negative edge weight)

	unsigned int vertexCount = boost::num_vertices(tmpGraph);

	// init predecessors and distances
	std::vector<cpGraph::vertex_descriptor> predecessors(vertexCount);
	for (unsigned int i=0; i<vertexCount; i++)
		predecessors[i] = i;
	std::vector<int> distances(vertexCount);
	distances[0] = 0;

	// gets the weight property
	boost::property_map<cpGraph, boost::edge_weight_t>::type weight_pmap = get(boost::edge_weight, tmpGraph);

	// call the algorithm
	bool bellmanFordSucceeded = bellman_ford_shortest_paths(
		tmpGraph,
		vertexCount,
		weight_map(weight_pmap).
		distance_map(&distances[0]).
		predecessor_map(&predecessors[0])
	);

	unsigned int pathCost = 0;
	if (!bellmanFordSucceeded)
		errs() << "ERROR: Bellman Ford could not be applied!\n";
	else
		pathCost = std::abs(*std::min_element(distances.begin(), distances.end()));
	
	return pathCost; 
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
		std::string comOps = "comOps: ";
		for (std::vector<CommunicationType>::iterator it = pGraph[*edgeIt].comOperations.begin(); 
			it != pGraph[*edgeIt].comOperations.end(); ++it) {
			std::ostringstream oss;
			oss << *it;
			comOps += oss.str() + " ";
		}
		Graph::vertex_descriptor u = boost::source(*edgeIt, pGraph), v = boost::target(*edgeIt, pGraph);
		errs() << pGraph[u].name << " -> " << pGraph[v].name << " (" << comOps << ")\n";
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
		std::string comOps = "comOps: ";
		for (std::vector<CommunicationType>::iterator it = pGraph[*edgeIt].comOperations.begin(); 
			it != pGraph[*edgeIt].comOperations.end(); ++it) {
			std::ostringstream oss;
			oss << *it;
			comOps += oss.str() + " ";
		}
		Graph::vertex_descriptor u = boost::source(*edgeIt, pGraph), v = boost::target(*edgeIt, pGraph);
		dotfile << "  " << pGraph[u].name << "->" << pGraph[v].name << "[label=\"" << comOps << "\"];\n";
	}
	dotfile << "}";
}


void PartitioningGraph::printPartitions(void) {
	std::map<unsigned int, std::vector<VertexDescriptor> > partitions;
	VertexIterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(pGraph); vertexIt != vertexEnd; ++vertexIt)
		partitions[pGraph[*vertexIt].partition].push_back(*vertexIt);
	std::map<unsigned int, std::vector<VertexDescriptor> >::iterator it;
	errs() << "Partitioning Graph Partitions: \n";
	for (it = partitions.begin(); it != partitions.end(); ++it) {
		errs() << it->first << ": [ ";
		for (std::vector<VertexDescriptor>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			errs() << pGraph[*it2].name << " ";
		errs() << "]\n";
	}
	errs() << "\n";
}
