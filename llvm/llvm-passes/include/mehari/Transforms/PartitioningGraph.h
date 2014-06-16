#ifndef PARTITIONING_GRAPH_H
#define PARTITIONING_GRAPH_H

#include "llvm/IR/Function.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"
#include "mehari/HardwareInformation.h"

#include <boost/graph/adjacency_list.hpp> 
#include <boost/graph/graphviz.hpp>
#include <boost/tuple/tuple.hpp>

#include <vector>


using namespace llvm;

class PartitioningGraph {

public:
	
	PartitioningGraph();
	~PartitioningGraph();

	// overwrite copy constructor and assignment operator to create a deep copy of PartitioningGraphs
	PartitioningGraph(const PartitioningGraph &cSource);
	PartitioningGraph &operator=(const PartitioningGraph &cSource);


	void create(std::vector<Instruction*> &instructions, InstructionDependencyList &dependencies);


	struct ComputationUnit {
		std::string name;
		unsigned int partition;
		std::vector<Instruction*> instructions;
	};

	struct Communication {
		std::vector<CommunicationType> comOperations;
	};

	typedef boost::adjacency_list<
		boost::setS, 				// store out-edges of each vertex in a set to avoid parallel edges
		boost::vecS, 				// store vertex set in a std::vector
		boost::bidirectionalS, 		// the graph has got bidirected edges (required for boost::in_edges)
		ComputationUnit,			// the vertices are of type ComputationUnit
		Communication				// the edges are of type Communication
		> Graph;

	typedef Graph::vertex_iterator VertexIterator;
	typedef Graph::vertex_descriptor VertexDescriptor;
	typedef Graph::edge_iterator EdgeIterator;
	typedef Graph::edge_descriptor EdgeDescriptor;

	unsigned int getVertexCount(void);

	VertexDescriptor getRandomVertex(void);

	VertexIterator getFirstIterator();
	VertexIterator getEndIterator(); 

	EdgeIterator getFirstEdgeIterator();
	EdgeIterator getEndEdgeIterator();

	VertexDescriptor getSourceVertex(EdgeDescriptor ed);
	VertexDescriptor getTargetVertex(EdgeDescriptor ed);

	std::string getName(VertexDescriptor vd);

	void setPartition(VertexDescriptor vd, unsigned int partition);
	unsigned int getPartition(VertexDescriptor vd);

	void setInstructions(VertexDescriptor vd, std::vector<Instruction*> &instructions);
	std::vector<Instruction*> &getInstructions(VertexDescriptor vd);

	unsigned int getCommunicationCost(VertexDescriptor vd1, VertexDescriptor vd2, 
		std::string &sourceDevice, std::string &targetDevice);
	unsigned int getDeviceIndependentCommunicationCost(VertexDescriptor vd1, VertexDescriptor vd2);
	unsigned int getExecutionTime(VertexDescriptor vd, std::string &targetDevice);

	boost::tuple<unsigned int, unsigned int> getInternalExternalCommunicationCost(
		VertexDescriptor vd, std::vector<std::string> &partitioningDevices);

	unsigned int getCriticalPathCost(std::vector<std::string> &partitioningDevices);

	VertexDescriptor getVertexForInstruction(Instruction *instruction);

	void printGraph(std::string &name);
	void printGraphviz(Function &func, std::string &name, std::string &outputDir);
	void printPartitions(void);

private:

	void createVertices(std::vector<Instruction*> &instructions);
	void addEdges(InstructionDependencyList &dependencies);

	bool isCuttingInstr(Instruction *instr);

	void addInstructionsToList(std::vector<Instruction*> instructions);

	void copyGraph(const Graph &orig, Graph &copy);

	unsigned int calcEdgeCost(EdgeDescriptor ed, std::string &sourceDevice, std::string &targetDevice);
	unsigned int calcDeviceIndependentEdgeCost(EdgeDescriptor ed);

	std::vector<Instruction*> instructionList;
	Graph pGraph;
	VertexDescriptor initVertex;
};

#endif /*PARTITIONING_GRAPH_H*/
