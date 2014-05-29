#include "mehari/Transforms/PartitioningAlgorithms.h"

#include <math.h>	// for exp
#include <ctime> 	// for using srand with time 
#include <algorithm>

// DEBUG
#include "llvm/Support/raw_ostream.h"



// -----------------------------------
// Random Partitioning
// -----------------------------------

void RandomPartitioning::apply(PartitioningGraph &pGraph, unsigned int partitionCount) {
	srand(42);
	PartitioningGraph::VertexIterator vIt = pGraph.getFirstIterator(); 
	PartitioningGraph::VertexIterator vEnd = pGraph.getEndIterator();
	for (; vIt != vEnd; ++vIt) {
		PartitioningGraph::VertexDescriptor vd = *vIt;
		pGraph.setPartition(vd, rand()%partitionCount);
	}
}


// -----------------------------------
// Hierarchical Clustering
// -----------------------------------

void HierarchicalClustering::apply(PartitioningGraph &pGraph, unsigned int partitionCount) {
	partitioningGraph = pGraph;
	// first create a vertex in clustering graph for each of the functional units in the partitioning graph
	PartitioningGraph::VertexIterator pvIt;
	PartitioningGraph::VertexIterator pvBegin = pGraph.getFirstIterator(); 
	PartitioningGraph::VertexIterator pvEnd = pGraph.getEndIterator();
	for (pvIt = pvBegin; pvIt != pvEnd; ++pvIt) {
		PartitioningGraph::VertexDescriptor pvd = *pvIt;
		FunctionalUnitList fuList;
		fuList.push_back(pvd);
		VertexDescriptor newVertex = boost::add_vertex(clusteringGraph);
		clusteringGraph[newVertex].funcUnits = fuList;
	}

	// second add edges between all of the vertices of the clustering graph and set initial closeness
	VertexIterator vIt1, vIt2, vBegin, vEnd;
	boost::tie(vBegin, vEnd) = boost::vertices(clusteringGraph);
	for (vIt1 = vBegin; vIt1 != vEnd; ++vIt1) {
		VertexDescriptor vd1 = *vIt1;
		for (vIt2 = vIt1; vIt2 != vEnd; ++vIt2) {
			if (vIt2 == vIt1)
				continue; // we do not want edge from a vertex to itself
			VertexDescriptor vd2 = *vIt2;
			EdgeDescriptor ed;
			bool inserted;
			boost::tie(ed, inserted) = boost::add_edge(vd1, vd2, clusteringGraph);
			boost::tie(clusteringGraph[ed].closeness, clusteringGraph[ed].pSizeProduct) = closenessFunction(vd1, vd2);
		}
	}

	// finally perform clustering and save all partitioning results that does not exceed the partitioning count
	std::vector<PartitioningGraph> partitioningResults;
	while (boost::num_vertices(clusteringGraph) > 1) {
		// DEBUG: print graph for each step
		// printGraph();
		VertexDescriptor vd1, vd2, vdnew;
		// find best pair of vertices given by closeness
		boost::tie(vd1, vd2) = getPairMaxCloseness();
		// merge this pair to one new vertex
		vdnew = mergeVertices(vd1, vd2);
		// update the edges to connect the new vertex and remove the outdated vertices
		updateEdges(vd1, vd2, vdnew);
		removeOutdatedVertices(vd1, vd2);
		// update the closeness values
		updateCloseness();
		// save the current partitioning result if it does not exceed the partitioning count
		if (boost::num_vertices(clusteringGraph) <= partitionCount) {
			PartitioningGraph tmpPartitioningResult = partitioningGraph;
			applyClusteringOnPartitioningGraph(clusteringGraph, tmpPartitioningResult);
			partitioningResults.push_back(tmpPartitioningResult);
		}
	}

	// determine the result with the minimum cost and save it in the original partitioning graph	
	// TODO: currently it is not implemented in the partitioning path to handle partition counts
	// 		 that differ from the given one -> clustering should only return #partitionCount partitions yet
	// pGraph = *std::min_element(partitioningResults.begin(), partitioningResults.end(), comparePartitioningResults);
	pGraph = *partitioningResults.begin();

	// DEBUG: print final graph
	printGraph();
}


// overwrite greater operator for graph edges to customize it
bool HierarchicalClustering::Edge::operator> (const Edge &e) const {
	if (closeness != e.closeness)
		// the closeness values are not equal, so we can choose the greater one
		return closeness > e.closeness;
	else
		// NOTE: a lower partition size product results in a higher closeness 
		return (pSizeProduct < e.pSizeProduct);
}


boost::tuple<float, unsigned int> HierarchicalClustering::closenessFunction(VertexDescriptor vd1, VertexDescriptor vd2) {
	// NOTE: for this closeness function the so called ratio cut metric ist used
	unsigned int comCost = 0, pExecTime1 = 0, pExecTime2 = 0;
	FunctionalUnitList funits1 = clusteringGraph[vd1].funcUnits;
	FunctionalUnitList funits2 = clusteringGraph[vd2].funcUnits;
	for (FunctionalUnitList::iterator it1 = funits1.begin(); it1 != funits1.end(); ++it1) {
		pExecTime1 += partitioningGraph.getExecutionTime(*it1);
		for (FunctionalUnitList::iterator it2 = funits2.begin(); it2 != funits2.end(); ++it2)
			comCost += partitioningGraph.getCommunicationCost(*it1, *it2);
	}
	for (FunctionalUnitList::iterator it2 = funits2.begin(); it2 != funits2.end(); ++it2)
		pExecTime2 += partitioningGraph.getExecutionTime(*it2);

	unsigned int pSizeProduct = pExecTime1 * pExecTime2;
	float closeness = float(comCost) / float(pSizeProduct);

	return boost::make_tuple(closeness, pSizeProduct);
}


boost::tuple<HierarchicalClustering::VertexDescriptor, HierarchicalClustering::VertexDescriptor> 
HierarchicalClustering::getPairMaxCloseness(void) {
	// find edge with maximum weight (closeness value)
	EdgeIterator edgeIt, edgeEnd;
	boost::tie(edgeIt, edgeEnd) = boost::edges(clusteringGraph);
	EdgeIterator edgeMaxIt = edgeIt;
	for (; edgeIt != edgeEnd; ++edgeIt) {
		if (clusteringGraph[*edgeIt] > clusteringGraph[*edgeMaxIt])
			edgeMaxIt = edgeIt;
	}
	// get vertices that are connected by this edge
	VertexDescriptor vd1 = boost::source(*edgeMaxIt, clusteringGraph);
	VertexDescriptor vd2 = boost::target(*edgeMaxIt, clusteringGraph);

	return boost::make_tuple(vd1, vd2);
}


HierarchicalClustering::VertexDescriptor HierarchicalClustering::mergeVertices(VertexDescriptor vd1, VertexDescriptor vd2) {
	// merge lists of functional units
	FunctionalUnitList l1, l2, lnew;
	l1 = clusteringGraph[vd1].funcUnits;
	l2 = clusteringGraph[vd2].funcUnits;
	lnew.insert(lnew.end(), l1.begin(), l1.end());
	lnew.insert(lnew.end(), l2.begin(), l2.end());
	// create new vertex and return it's descriptor
	VertexDescriptor newVertex = boost::add_vertex(clusteringGraph);
	clusteringGraph[newVertex].funcUnits = lnew;
	return newVertex;
}


void HierarchicalClustering::updateEdges(VertexDescriptor vd1, VertexDescriptor vd2, VertexDescriptor vdnew) {
	VertexIterator vIt, vEnd;
	boost::tie(vIt, vEnd) = boost::vertices(clusteringGraph);
	for (; vIt != vEnd; ++vIt) {
		VertexDescriptor vd = *vIt;
		if (vd == vd1 || vd == vd2 || vd == vdnew)
			continue;
		// calculate closeness for new edge (average of the previous ones)
		EdgeDescriptor ed1 = boost::edge(vd, vd1, clusteringGraph).first;
		EdgeDescriptor ed2 = boost::edge(vd, vd2, clusteringGraph).first;
		float newCloseness = (clusteringGraph[ed1].closeness + clusteringGraph[ed2].closeness) / 2;
		// create new edge between current vertex and new vertex
		EdgeDescriptor ed;
		bool inserted;
		boost::tie(ed, inserted) = boost::add_edge(vd, vdnew, clusteringGraph);
		clusteringGraph[ed].closeness = newCloseness;
	}
}


void HierarchicalClustering::removeOutdatedVertices(VertexDescriptor vd1, VertexDescriptor vd2) {
	boost::clear_vertex(vd1, clusteringGraph);
	boost::clear_vertex(vd2, clusteringGraph);
	boost::remove_vertex(vd1, clusteringGraph);
	boost::remove_vertex(vd2, clusteringGraph);
}


void HierarchicalClustering::updateCloseness(void) {
	EdgeIterator edgeIt, edgeEnd;
	for (boost::tie(edgeIt, edgeEnd) = boost::edges(clusteringGraph); edgeIt != edgeEnd; ++edgeIt) {
		VertexDescriptor u = boost::source(*edgeIt, clusteringGraph); 
		VertexDescriptor v = boost::target(*edgeIt, clusteringGraph);
		boost::tie(clusteringGraph[*edgeIt].closeness, clusteringGraph[*edgeIt].pSizeProduct) = closenessFunction(u, v);
	}
}


void HierarchicalClustering::applyClusteringOnPartitioningGraph(Graph &cGraph, PartitioningGraph &pGraph) {
	// save result by setting partitions
	VertexIterator vtIt, vtEnd;
	boost::tie(vtIt, vtEnd) = boost::vertices(cGraph);
	for (int i=0; vtIt != vtEnd; ++vtIt, i++) {
		FunctionalUnitList flst = cGraph[*vtIt].funcUnits;
		for (FunctionalUnitList::iterator fIt = flst.begin(); fIt != flst.end(); ++fIt)
			pGraph.setPartition(*fIt, i);
	}
}


bool HierarchicalClustering::comparePartitioningResults(PartitioningGraph &pGraph1, PartitioningGraph &pGraph2) {
	return pGraph1.getCriticalPathCost() < pGraph2.getCriticalPathCost();
}


void HierarchicalClustering::printGraph(void) {
	errs() << "Clustering Graph: \n";
	errs() << "\nVERTICES:\n";
	VertexIterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(clusteringGraph); vertexIt != vertexEnd; ++vertexIt) { 
		errs() << "vertex " << *vertexIt << ": [ ";
		FunctionalUnitList flst = clusteringGraph[*vertexIt].funcUnits;
		for (FunctionalUnitList::iterator it = flst.begin(); it != flst.end(); ++it)
			errs() << partitioningGraph.getName(*it) << " ";
		errs() << "]\n";
	}
	errs() << "\nEDGES:\n";
	EdgeIterator edgeIt, edgeEnd;
	for (boost::tie(edgeIt, edgeEnd) = boost::edges(clusteringGraph); edgeIt != edgeEnd; ++edgeIt) {
		VertexDescriptor u = boost::source(*edgeIt, clusteringGraph), v = boost::target(*edgeIt, clusteringGraph);
		errs() << u << " -- " << v << " (closeness: " << clusteringGraph[*edgeIt].closeness << ")\n";
	}
}


// -----------------------------------
// Simulated Annealing
// -----------------------------------

void SimulatedAnnealing::apply(PartitioningGraph &pGraph, unsigned int partitionCount) {
	// configure algorithm
	Tinit = 1.0;
	Tmin = 0.1;
	iterationMax = 2000;
	tempAcceptenceMultiplicator = 1.0;
	tempDecreasingFactor = 0.9;

	numOfPartitions = partitionCount;

	// create initial state
	RandomPartitioning P;
	P.apply(pGraph, partitionCount);

	// run simulated annealing algorithm
	simulatedAnnealing(pGraph, Tinit);
}


void SimulatedAnnealing::simulatedAnnealing(State &state, Temperature initialTemperature) {
	State S = state;
	Temperature T = initialTemperature;
	
	srand(time(0));

	while (!frozen(T)) {
		int itCount = 0;
		while (!equilibrium(itCount)) {
			State newS = S;
			randomMove(newS);
			int deltaCost = costFunction(newS) - costFunction(S);
			if (acceptNewState(deltaCost, T) > randomNumber())
				S = newS;
			itCount++;
		}
		T = decreaseTemperature(T);
	}
	state = S;
}


bool SimulatedAnnealing::frozen(Temperature temp) {
	return temp <= Tmin; // TODO: or no improvement
}


bool SimulatedAnnealing::equilibrium(int iterationCount) {
	return iterationCount >= iterationMax; // TODO: or no improvement
}


float SimulatedAnnealing::acceptNewState(int deltaCost, Temperature T) {
	return std::min(1.0, exp((-1)*float(deltaCost)/(tempAcceptenceMultiplicator*T)));
}


SimulatedAnnealing::Temperature SimulatedAnnealing::decreaseTemperature(Temperature T) {
	return tempDecreasingFactor * T;
}


void SimulatedAnnealing::randomMove(State &state) {
	// NOTE: the state is stored in a PartitioningGraph
	PartitioningGraph::VertexDescriptor vd = state.getRandomVertex();
	unsigned int oldPartition = state.getPartition(vd);
	unsigned int newPartition;
	do {
		newPartition = rand() % numOfPartitions;
	} while(newPartition == oldPartition);
	state.setPartition(vd, newPartition);
}


int SimulatedAnnealing::costFunction(State &state) {
	// NOTE: the state is stored in a PartitioningGraph
	return state.getCriticalPathCost();
}

double SimulatedAnnealing::randomNumber(void) {
	return (rand() / double(RAND_MAX));
}
