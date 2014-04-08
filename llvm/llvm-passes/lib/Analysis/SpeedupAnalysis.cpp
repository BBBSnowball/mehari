#include "mehari/Analysis/SpeedupAnalysis.h"
#include "mehari/HardwareInformation.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/CommandLine.h"

#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/graphviz.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <vector>
#include <algorithm>


static cl::opt<bool> Graphviz ("dot", cl::desc("Enable writing the dependency graph to a graphviz file."));
static cl::opt<std::string> TargetFunctions("speedup-functions", 
            cl::desc("Specify the functions the speedup analysis will be applyed on (seperated by whitespace)"), 
            cl::value_desc("target-functions"));


SpeedupAnalysis::SpeedupAnalysis() : FunctionPass(ID) {
  initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
  parseTargetFunctions();
}

SpeedupAnalysis::~SpeedupAnalysis() {}


bool SpeedupAnalysis::runOnFunction(Function &func) {

  std::string functionName = func.getName().str();

  // just handle those functions specified by the command line parameter
  if (std::find(targetFunctions.begin(), targetFunctions.end(), functionName) == targetFunctions.end())
    return false;
  
  errs() << "\n\nspeedup analysis: " << func.getName() << "\n";

  // create worklist containing all instructions of the function
  worklist.clear();
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  // run InstructionDependencyAnalysis
  InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>();
  InstructionDependencyList dependencies = IDA->getDependencies(func);

  // convert InstructionDependencyList to a graph
  buildDependencyGraph(dependencies);


  // determine critical path (which is the shortest path because of the negative edge weights)

  // init distances and predecessors
  std::vector<Graph::vertex_descriptor> predecessors(boost::num_vertices(depGraph));
  for (int i = 0; i < boost::num_vertices(depGraph); ++i)
    predecessors[i] = i;

  // init distances vector
  std::vector<int> distances(boost::num_vertices(depGraph));

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


  // create graph plot
  if (Graphviz)
    printGraphviz(functionName);


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

  return false;
}


void SpeedupAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredTransitive<InstructionDependencyAnalysis>();
  AU.setPreservesAll();
}


void SpeedupAnalysis::parseTargetFunctions() {
  boost::algorithm::split(targetFunctions, TargetFunctions, boost::algorithm::is_any_of(" "));
}


void SpeedupAnalysis::buildDependencyGraph(InstructionDependencyList &dependencies) {
  int index = 0;
  for (InstructionDependencyList::iterator instrDepIt = dependencies.begin(); 
        instrDepIt != dependencies.end(); ++instrDepIt, index++) {
    for (InstructionDependencies::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
      boost::add_edge(int(*depNumIt), int(index), (-1)*(getInstructionCost(worklist[*depNumIt])), depGraph);
    }
  }
  // add an edge from the first vertex to each vertex of the graph that has no predecessor
  Graph::vertex_iterator vertexIt, vertexEnd;
  Graph::in_edge_iterator inedgeIt, inedgeEnd;
  for (boost::tie(vertexIt, vertexEnd) = boost::vertices(depGraph); vertexIt != vertexEnd; ++vertexIt) {
    tie(inedgeIt, inedgeEnd) = in_edges(*vertexIt, depGraph);
    if (inedgeIt == inedgeEnd)
      boost::add_edge(0, *vertexIt, 0, depGraph);
  }
}


void SpeedupAnalysis::printGraphviz(std::string &name) {
  std::map<std::string, std::string> graph_attr, vertex_attr, edge_attr;
  vertex_attr["shape"] = "rectangle";

  std::string vertex_names[int(worklist.size())];
  for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) {
    int instr_number = instr_it-worklist.begin();
    Instruction *instr = dyn_cast<Instruction>(*instr_it);
    llvm::raw_string_ostream rso(vertex_names[instr_number]);
    instr->print(rso);
  }

  boost::property_map<Graph, boost::edge_weight_t>::type trans_delay = get(boost::edge_weight, depGraph);

  std::string fileName = "_output/graph/speedup-analysis-graph-" + name + ".dot";
  std::ofstream dotfile(fileName.c_str());
  boost::write_graphviz(dotfile, depGraph,
                          boost::make_label_writer(vertex_names),
                          boost::make_label_writer(trans_delay),
                          boost::make_graph_attributes_writer(graph_attr, vertex_attr, edge_attr));
}


unsigned int SpeedupAnalysis::getInstructionCost(Instruction *instruction) {
  HardwareInformation hwInfo;
  DeviceInformation *devInfo = hwInfo.getDeviceInfo("Cortex-A9");
  InstructionInformation *instrInfo = devInfo->getInstructionInfo(instruction->getOpcodeName());
  if (instrInfo == NULL) {
    errs() << "WARNING: unhandled instruction (" << instruction->getOpcode() << " / " << instruction->getOpcodeName() << ")\n";
    return 1;
  }
  return instrInfo->getCycleCount();
}


// register pass so we can call it using opt
char SpeedupAnalysis::ID = 0;
static RegisterPass<SpeedupAnalysis>
Y("speedup", "Analysis of the possible speedup by executing instructions in parallel as much as possible.");
