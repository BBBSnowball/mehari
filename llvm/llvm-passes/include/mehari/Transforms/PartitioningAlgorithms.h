#ifndef PARTITIONING_ALGORITHMS_H
#define PARTITIONING_ALGORITHMS_H

#include "mehari/Transforms/PartitioningGraph.h"

#include <boost/graph/adjacency_list.hpp> 
#include <boost/tuple/tuple.hpp>

#include <vector>


class AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, unsigned int partitionCount) = 0;
};



class RandomPartitioning : public AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, unsigned int partitionCount);
};



class HierarchicalClustering : public AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, unsigned int partitionCount);

private:
	typedef std::vector<PartitioningGraph::VertexDescriptor> FunctionalUnitList;
	struct Vertex { FunctionalUnitList funcUnits; };
	struct Edge { 
		float closeness; 
		unsigned int pSizeProduct;
		bool operator> (const Edge &) const;
	};

	typedef boost::adjacency_list<
		boost::setS, 			// store out-edges of each vertex in a set to avoid parallel edges
		boost::listS, 			// store vertex set in a std::list
		boost::undirectedS, 	// the graph has got directed edges
		Vertex,					// the vertex type
		Edge					// the edge type
		> Graph;

public:
	typedef Graph::vertex_iterator VertexIterator;
	typedef Graph::vertex_descriptor VertexDescriptor;
	typedef Graph::edge_iterator EdgeIterator;
	typedef Graph::edge_descriptor EdgeDescriptor;

private:
	Graph clusteringGraph;
	PartitioningGraph partitioningGraph;

	boost::tuple<float, unsigned int> closenessFunction(VertexDescriptor vd1, VertexDescriptor vd2);

	boost::tuple<VertexDescriptor, VertexDescriptor> getPairMaxCloseness(void);
	VertexDescriptor mergeVertices(VertexDescriptor vd1, VertexDescriptor vd2);
	void updateEdges(VertexDescriptor vd1, VertexDescriptor vd2, VertexDescriptor vdnew);
	void removeOutdatedVertices(VertexDescriptor vd1, VertexDescriptor vd2);
	void updateCloseness(void);

	void applyClusteringOnPartitioningGraph(PartitioningGraph &pGraph);
	boost::tuple<PartitioningGraph, unsigned int> getFinalResult(
		std::vector<PartitioningGraph> &partitioningResults, unsigned int maxPartitionCount);

	void printGraph(void);
};



class SimulatedAnnealing : public AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, unsigned int partitionCount);

	typedef PartitioningGraph State;
	typedef float Temperature;

private:
	void simulatedAnnealing(State &state, Temperature initialTemperature);

	bool frozen(Temperature temp);
	bool equilibrium(int iterationCount);
	float acceptNewState(int deltaCost, Temperature T);
	Temperature decreaseTemperature(Temperature T);

	void randomMove(State &state);
	int costFunction(State &state);
	double randomNumber(void);

	Temperature Tinit, Tmin;
	int iterationMax;
	float tempAcceptenceMultiplicator;
	float tempDecreasingFactor;

	unsigned int numOfPartitions;
};


#endif /*PARTITIONING_ALGORITHMS_H*/
