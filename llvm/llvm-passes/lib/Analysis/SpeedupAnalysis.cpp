#include "mehari/Analysis/SpeedupAnalysis.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/CommandLine.h"

#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/graphviz.hpp>

#include <vector>
#include <algorithm>


static cl::opt<bool> Graphviz ("dot", cl::desc("Enable writing the dependency graph to a graphviz file."));


SpeedupAnalysis::SpeedupAnalysis() : FunctionPass(ID) {}
SpeedupAnalysis::~SpeedupAnalysis() {}


bool SpeedupAnalysis::runOnFunction(Function &func) {

  errs() << "\n\nspeedup analysis: " << func.getName() << "\n";

  // create worklist containing all instructions of the function
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  // run InstructionDependencyAnalysis
  InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>();
  InstructionDependencyList dependencies = IDA->getDependencies(worklist);

  // convert InstructionDependencyList to a graph
  buildDependencyGraph(dependencies);


  // add a root node for each vertex of the graph that has no predecessor
  Graph::vertex_iterator vertexIt, vertexEnd;
  Graph::in_edge_iterator inedgeIt, inedgeEnd;
  Graph::vertex_descriptor root = boost::add_vertex(depGraph);

  for (boost::tie(vertexIt, vertexEnd) = boost::vertices(depGraph); vertexIt != vertexEnd; ++vertexIt) {
    tie(inedgeIt, inedgeEnd) = in_edges(*vertexIt, depGraph);
    if (inedgeIt == inedgeEnd && *vertexIt != root)
      boost::add_edge(root, *vertexIt, 0, depGraph);
  }
  
  // determine critical path (which is the shortest path because of the negative edge weights)

  // init distances and predecessors
  std::vector<Graph::vertex_descriptor> predecessors(boost::num_vertices(depGraph));
  for (int i = 0; i < boost::num_vertices(depGraph); ++i)
    predecessors[i] = i;

  std::vector<int> distances(boost::num_vertices(depGraph));
  distances[root] = 0;

  // gets the weight property
  boost::property_map<Graph, boost::edge_weight_t>::type weight_pmap = get(boost::edge_weight, depGraph);  

  // call the algorithm
  bool bellmanFordSucceeded = bellman_ford_shortest_paths(
    depGraph, 
    boost::num_vertices(depGraph), 
    weight_map(weight_pmap).
      distance_map(&distances[0]).
        predecessor_map(&predecessors[0])
    );

  if (!bellmanFordSucceeded) {
    errs() << "ERROR: Could not apply the Bellman Ford algorithm on the instruction dependency graph.\n\n";
    return false;
  }


  // print results of the speedup analysis
  unsigned int T_s = 0;
  for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) 
    T_s += getInstructionCost(dyn_cast<Instruction>(*instr_it));
  unsigned int T_p = std::abs(*std::min_element(distances.begin(), distances.end()));
  errs() << "\n";
  errs() << "runtime for computing all instructions sequentially: " << T_s << "\n"; 
  errs() << "runtime for computing instructions in parallel: " << T_p << "\n"; 
  errs() << "\n" << "speedup: " << format("%4.4f", float(T_s)/float(T_p)) << "\n";


  if (Graphviz)
    printGraphviz();

  return false;
}


void SpeedupAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredTransitive<InstructionDependencyAnalysis>();
  AU.setPreservesAll();
}


void SpeedupAnalysis::buildDependencyGraph(InstructionDependencyList &dependencies) {
  int index = 0;
  for (InstructionDependencyList::iterator instrDepIt = dependencies.begin(); 
        instrDepIt != dependencies.end(); ++instrDepIt, index++) {
    for (InstructionDependencies::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
      boost::add_edge(int(*depNumIt), int(index), (-1)*(getInstructionCost(worklist[*depNumIt])), depGraph);
    }
  }
}


void SpeedupAnalysis::printGraphviz() {
  std::map<std::string, std::string> graph_attr, vertex_attr, edge_attr;
  vertex_attr["shape"] = "rectangle";

  std::string vertex_names[int(worklist.size())];
  for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) {
    int instr_number = instr_it-worklist.begin();
    Instruction *instr = dyn_cast<Instruction>(*instr_it);
    llvm::raw_string_ostream rso(vertex_names[instr_number]);
    instr->print(rso);
  }
  vertex_names[worklist.size()-1] = "ROOT";

  boost::property_map<Graph, boost::edge_weight_t>::type trans_delay = get(boost::edge_weight, depGraph);

  std::ofstream dotfile("_output/speedup-analysis-graph.dot");
  boost::write_graphviz(dotfile, depGraph,
                          boost::make_label_writer(vertex_names),
                          boost::make_label_writer(trans_delay),
                          boost::make_graph_attributes_writer(graph_attr, vertex_attr, edge_attr));
}


// TODO there should be a better way to determine the instruction cost
unsigned int SpeedupAnalysis::getInstructionCost(Instruction *instruction) {
  switch(int(instruction->getType()->getTypeID())) {
    case 0:  // store instruction
      return 2;
      break;
    case 3:  // arithmetic operation
      return 4;
      break;
    case 14: // load instruction
      return 3;
      break;
    default:
      errs() << "ERROR: unhandled instruction: " << *instruction << "\n"; 
      return 1;
      break;
  }
}


// register pass so we can call it using opt
char SpeedupAnalysis::ID = 0;
static RegisterPass<SpeedupAnalysis>
Y("speedup", "Analysis of the possible speedup by executing instructions in parallel as much as possible.");
