#include "mehari/Transforms/PartitioningAlgorithms.h"

// DEBUG
#include "llvm/Support/raw_ostream.h"


// -----------------------------------
// RandomPartitioning
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
// HierarchicalClustering
// -----------------------------------

void HierarchicalClustering::apply(PartitioningGraph &pGraph, unsigned int partitionCount) {
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

	// second calculate the initial closeness and add edges between all of the vertices of the clustering graph
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
			clusteringGraph[ed].closeness = closenessFunction(pGraph, vd1, vd2);
		}
	}

	// third perform clustering until the requested number of partitions is reached
	while (boost::num_vertices(clusteringGraph) > partitionCount) {
		// DEBUG: print graph for each step
		// printGraph();
		VertexDescriptor vd1, vd2, vdnew;
		boost::tie(vd1, vd2) = getPairMaxCloseness();
		vdnew = mergeVertices(vd1, vd2);
		updateEdges(vd1, vd2, vdnew);
		removeOutdatedVertices(vd1, vd2);
	}

	// finally save result by setting partitions
	VertexIterator vtIt, vtEnd;
	boost::tie(vtIt, vtEnd) = boost::vertices(clusteringGraph);
	for (int i=0; vtIt != vtEnd; ++vtIt, i++) {
		FunctionalUnitList flst = clusteringGraph[*vtIt].funcUnits;
		for (FunctionalUnitList::iterator fIt = flst.begin(); fIt != flst.end(); ++fIt)
			pGraph.setPartition(*fIt, i);
	}

	// DEBUG: print final graph
	printGraph();
}


float HierarchicalClustering::closenessFunction(PartitioningGraph &pGraph, VertexDescriptor vd1, VertexDescriptor vd2) {
	PartitioningGraph::VertexDescriptor pvd1 = clusteringGraph[vd1].funcUnits.front();
	PartitioningGraph::VertexDescriptor pvd2 = clusteringGraph[vd2].funcUnits.front();
	unsigned int comCost = pGraph.getCommunicationCost(pvd1, pvd2);
	float closeness = (comCost > 0 ? (1/comCost) : comCost);
	return closeness;
}


boost::tuple<HierarchicalClustering::VertexDescriptor, HierarchicalClustering::VertexDescriptor> 
HierarchicalClustering::getPairMaxCloseness(void) {
	// find edge with maximum weight (closeness value)
	EdgeIterator edgeIt, edgeEnd;
	boost::tie(edgeIt, edgeEnd) = boost::edges(clusteringGraph);
	EdgeIterator edgeMaxIt = edgeIt;
	for (; edgeIt != edgeEnd; ++edgeIt) {
		if (clusteringGraph[*edgeIt].closeness > clusteringGraph[*edgeMaxIt].closeness)
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
		// calculate closeness for new edge
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


void HierarchicalClustering::printGraph(void) {
	errs() << "Clustering Graph: \n";
	errs() << "\nVERTICES:\n";
	VertexIterator vertexIt, vertexEnd;
	for (boost::tie(vertexIt, vertexEnd) = boost::vertices(clusteringGraph); vertexIt != vertexEnd; ++vertexIt) { 
		errs() << "vertex " << *vertexIt << ": [ ";
		FunctionalUnitList flst = clusteringGraph[*vertexIt].funcUnits;
		for (FunctionalUnitList::iterator it = flst.begin(); it != flst.end(); ++it)
			errs() << *it << " ";
		errs() << "]\n";
	}
	errs() << "\nEDGES:\n";
	EdgeIterator edgeIt, edgeEnd;
	for (boost::tie(edgeIt, edgeEnd) = boost::edges(clusteringGraph); edgeIt != edgeEnd; ++edgeIt) {
		VertexDescriptor u = boost::source(*edgeIt, clusteringGraph), v = boost::target(*edgeIt, clusteringGraph);
		errs() << u << " -- " << v << " (closeness: " << clusteringGraph[*edgeIt].closeness << ")\n";
	}
}
