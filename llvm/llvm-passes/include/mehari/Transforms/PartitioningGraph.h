#include "llvm/IR/Function.h"

#include <boost/graph/adjacency_list.hpp> 

#include <vector>

using namespace llvm;

class PartitioningGraph {

public:
	PartitioningGraph();
	~PartitioningGraph();

	void buildGraph(Function &func);
	void printGraph(void);

private:
	void createVertices(Function &func);
	void addEdges();

	struct ComputationUnit {
		std::string name;
		std::vector<Instruction*> instructions;
	};

	struct Communication {
		float cost;
	};

	typedef boost::adjacency_list<
		boost::listS, // store out-edges of each vertex in a std::list
		boost::listS, // store vertex set in a std::list
		boost::directedS, // the graph is directed to represend dependencies
		ComputationUnit,
		Communication
		> Graph;

	Graph pGraph;
};