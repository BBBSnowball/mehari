#include "mehari/Analysis/IRGraphPrinter.h"
#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include <boost/graph/adjacency_list.hpp> 
#include <boost/graph/graphviz.hpp>

#include <vector>
#include <algorithm>


IRGraphPrinter::IRGraphPrinter() : FunctionPass(ID) {
  initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
}

IRGraphPrinter::~IRGraphPrinter() {}


bool IRGraphPrinter::runOnFunction(Function &func) {
  std::string functionName = func.getName().str();
  std::string fileName = "_output/graph/dataflow-graph-" + functionName + ".dot";
  printDataflowGraph(fileName, func);
  return false;
}


void IRGraphPrinter::printDataflowGraph(std::string &filename, Function &func) {
  // create worklist containing all instructions of the function
  std::vector<Instruction*> worklist;
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  // run InstructionDependencyAnalysis
  InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>();
  InstructionDependencyList dependencies = IDA->getDependencies(worklist);

  // create new graph  
  typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS > Graph;
  Graph g;

  int index = 0;
  for (InstructionDependencyList::iterator instrDepIt = dependencies.begin(); 
        instrDepIt != dependencies.end(); ++instrDepIt, index++) {
    for (InstructionDependencies::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
      boost::add_edge(int(*depNumIt), int(index), g);
    }
  }

  // write graphviz
  std::map<std::string, std::string> graph_attr, vertex_attr, edge_attr;
  vertex_attr["shape"] = "rectangle";

  std::string vertex_names[int(worklist.size())];
  for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) {
    int instr_number = instr_it-worklist.begin();
    Instruction *instr = dyn_cast<Instruction>(*instr_it);
    llvm::raw_string_ostream rso(vertex_names[instr_number]);
    instr->print(rso);
  }

  std::ofstream dotfile(filename.c_str());
  boost::write_graphviz(dotfile, g, boost::make_label_writer(vertex_names));
}


void IRGraphPrinter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredTransitive<InstructionDependencyAnalysis>();
  AU.setPreservesAll();
}


// register pass so we can call it using opt
char IRGraphPrinter::ID = 0;
static RegisterPass<IRGraphPrinter>
Y("dfg", "Print dataflow graph using graphviz.");