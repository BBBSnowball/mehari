#ifndef PARTITIONING_GRAPH_H
#define PARTITIONING_GRAPH_H

#include "llvm/IR/Function.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include <boost/graph/adjacency_list.hpp> 
#include <boost/graph/graphviz.hpp>

#include <vector>


using namespace llvm;

class PartitioningGraph {

public:

	PartitioningGraph();
	~PartitioningGraph();


	struct ComputationUnit {
		std::string name;
		unsigned int partition;
		std::vector<Instruction*> instructions;
	};

	struct Communication {
		unsigned int cost;
	};

	typedef boost::adjacency_list<
		boost::setS, 				// store out-edges of each vertex in a set to avoid parallel edges
		boost::listS, 			// store vertex set in a std::list
		boost::directedS, 	// the graph has got directed edges
		ComputationUnit,		// the vertices are of type ComputationUnit
		Communication				// the edges are of type Communication
		> Graph;

	typedef Graph::vertex_iterator VertexIterator;
	typedef Graph::vertex_descriptor VertexDescriptor;

	VertexIterator getFirstIterator();
	VertexIterator getEndIterator(); 

	void setPartition(VertexDescriptor vd, unsigned int partition);

	void createVertices(std::vector<Instruction*> &instructions);
	void addEdges(InstructionDependencyList &dependencies);

	void printGraph(std::string &name);
	void printGraphviz(std::string &name);

private:

	Graph::vertex_descriptor getVertexForInstruction(Instruction *instruction);
	void addInstructionsToList(std::vector<Instruction*> instructions);

	std::vector<Instruction*> instructionList;
	Graph pGraph;
};

#endif /*PARTITIONING_GRAPH_H*/
