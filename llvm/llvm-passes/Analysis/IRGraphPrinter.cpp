#include "IRGraphPrinter.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"

#include <boost/graph/adjacency_list.hpp> 
#include <boost/graph/graphviz.hpp>

#include <vector>
#include <algorithm>


static cl::opt<bool> Data ("data", cl::desc("Create data flow graph."));
static cl::opt<bool> Control ("control", cl::desc("Create control flow graph."));

IRGraphPrinter::IRGraphPrinter() : FunctionPass(ID) {}
IRGraphPrinter::~IRGraphPrinter() {}


bool IRGraphPrinter::runOnFunction(Function &func) {
  if (Data)
    printDataflowGraph("_output/dataflow_graph.dot", func);
  if (Control)
    printControlflowGraph("_output/controlflow_graph.dot", func);

  return false;
}


void IRGraphPrinter::printDataflowGraph(std::string filename, Function &func) {
  typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS > Graph;

  Graph g;

  std::vector<Instruction*> worklist;
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  MemoryDependenceAnalysis &MDA = getAnalysis<MemoryDependenceAnalysis>();

  for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) {
    int instr_number = instr_it-worklist.begin();
    Instruction *instr = dyn_cast<Instruction>(*instr_it);

    // evaluate use-def chain to get all instructions the current instruction depends on
    for (User::op_iterator i = instr->op_begin(), e = instr->op_end(); i != e; ++i) {
      Instruction *op_inst = dyn_cast<Instruction>(*i);
      std::vector<Instruction*>::iterator it_pos = std::find(worklist.begin(), worklist.end(), &*op_inst);
      if (it_pos != worklist.end()) {
        boost::add_edge(int(it_pos-worklist.begin()), int(instr_number), g);
      }
    }
    // if the current instruction is a load instruction, find the associated store instruction
    if (instr->mayReadFromMemory()) {
      MemDepResult mdr = MDA.getDependency(instr);
    if (mdr.isClobber()) {
        std::vector<Instruction*>::iterator it_pos = std::find(worklist.begin(), worklist.end(), &*mdr.getInst());
        if (it_pos != worklist.end()) 
          boost::add_edge(int(it_pos-worklist.begin()), int(instr_number), g);
      }
    }
    // TODO what to do with 'ret void' ?
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


void IRGraphPrinter::printControlflowGraph(std::string filename, Function &func) {

}


void IRGraphPrinter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredTransitive<MemoryDependenceAnalysis>();
  AU.setPreservesAll();
}


// register pass so we can call it using opt
char IRGraphPrinter::ID = 0;
static RegisterPass<IRGraphPrinter>
Y("print-graph", "Print dataflow and control flow graphs using graphviz.");