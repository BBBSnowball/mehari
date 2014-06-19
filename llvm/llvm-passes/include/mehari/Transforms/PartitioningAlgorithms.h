#ifndef PARTITIONING_ALGORITHMS_H
#define PARTITIONING_ALGORITHMS_H

#include "mehari/Transforms/PartitioningGraph.h"

#include <boost/graph/adjacency_list.hpp> 
#include <boost/tuple/tuple.hpp>

#include <vector>


class AbstractPartitioningMethod {
public:
	virtual ~AbstractPartitioningMethod();
	virtual unsigned int apply(PartitioningGraph &pGraph, std::vector<std::string> &targetDevices) = 0;
};



class RandomPartitioning : public AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, std::vector<std::string> &targetDevices);
	void balancedBiPartitioning(PartitioningGraph &pGraph);
};



class HierarchicalClustering : public AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, std::vector<std::string> &targetDevices);

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

	std::vector<std::string> devices;

	boost::tuple<float, unsigned int> closenessFunction(VertexDescriptor vd1, VertexDescriptor vd2);
	boost::tuple<float, unsigned int> ratioCutCloseness(VertexDescriptor vd1, VertexDescriptor vd2);

	enum closenessMetric {
		ratioCut
	};

	static const closenessMetric usedCloseness = ratioCut;

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
	virtual unsigned int apply(PartitioningGraph &pGraph, std::vector<std::string> &targetDevices);

	typedef PartitioningGraph State;
	typedef float Temperature;

private:
	void simulatedAnnealing(State &state, Temperature initialTemperature);

	bool frozen(Temperature temp);
	bool equilibrium(int iterationCount);
	float acceptNewState(int deltaCost, Temperature T);
	Temperature decreaseTemperature(Temperature T);

	int costFunction(State &state);
	float normalizeCostDifference(int deltaCost, unsigned int K);

	void randomMove(State &state);
	double randomNumber(void);

	std::vector<std::string> devices;
	
	Temperature Tinit, Tmin;
	int iterationMax;
	float tempAcceptenceMultiplicator;
	float tempDecreasingFactor;

	unsigned int partitionCount;
};



class KernighanLin : public AbstractPartitioningMethod {
public:
	virtual unsigned int apply(PartitioningGraph &pGraph, std::vector<std::string> &targetDevices);

private:
	struct AdditionalVertexInfo {
		int costDifference;
		bool locked;
		AdditionalVertexInfo() : costDifference(0), locked(false) {};
	};

	std::vector<AdditionalVertexInfo> additionalVertexInformation;

	std::vector<std::string> devices;

	void createInitialCostDifferences(PartitioningGraph &pGraph);
	void updateCostDifferences(unsigned int icV1, unsigned int icV2, PartitioningGraph &pGraph);
	boost::tuple<unsigned int, unsigned int, unsigned int> findInterchangePair(PartitioningGraph &pGraph);
	void lockMovedVertices(unsigned int vd1, unsigned int vd2);
	void unlockAllVertices(void);
	boost::tuple<int, unsigned int> getMaxCostRecution(std::vector<int> &costReductions);
	void applyInterchanges(std::vector<boost::tuple<unsigned int, unsigned int> > &interchanges, 
		unsigned int iterationCount, PartitioningGraph &result);
};

#endif /*PARTITIONING_ALGORITHMS_H*/
