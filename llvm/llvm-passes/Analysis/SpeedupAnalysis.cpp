#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"

#include <vector>
#include <algorithm>

//#define PRINT_DEBUG

using namespace llvm;

namespace {

  struct MehariSpeedupAnalysis : public FunctionPass {
    static char ID; 
    MehariSpeedupAnalysis() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &func) {

      errs() << "\n\nfunction: " << func.getName() << "\n";

      std::vector<Instruction*> worklist;
      for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
        worklist.push_back(&*I);

      std::vector< std::vector<unsigned int> > ssa_dep_graph;
      for (int i=0; i<int(worklist.size()); i++)
        ssa_dep_graph.push_back(std::vector<unsigned int>(worklist.size()));

      MemoryDependenceAnalysis &MDA = getAnalysis<MemoryDependenceAnalysis>();

      for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) {
        int instr_number = instr_it-worklist.begin();
        Instruction *instr = dyn_cast<Instruction>(*instr_it);
        errs() << instr_number << "\t" << *instr << "\n";

        // dependencies between instructions:
        //  - use-def chain: all instructions that are used by the current instruction -> data dependences on SSA registers
        //  - MemoryDependenceAnalysis: memory dependencies between instructionsMeh -> data dependences on memory locations

        // evaluate use-def chain to get all instructions the current instruction depends on
        for (User::op_iterator i = instr->op_begin(), e = instr->op_end(); i != e; ++i) {
          Instruction *op_inst = dyn_cast<Instruction>(*i);
          std::vector<Instruction*>::iterator it_pos = std::find(worklist.begin(), worklist.end(), &*op_inst);
          if (it_pos != worklist.end()) {
            ssa_dep_graph[it_pos-worklist.begin()][instr_number] = 1; // TODO add time value depending on instr type
#ifdef PRINT_DEBUG
            errs() << "\t   --> use-def - depends on " << it_pos-worklist.begin() << ": " << *op_inst << "\n";
#endif
          }
        }
        // if the current instruction is a load instruction, find the associated store instruction
        if (instr->mayReadFromMemory()) {
          MemDepResult mdr = MDA.getDependency(instr);
          if (mdr.isClobber()) {
            std::vector<Instruction*>::iterator it_pos = std::find(worklist.begin(), worklist.end(), &*mdr.getInst());
            if (it_pos != worklist.end()) {
              ssa_dep_graph[it_pos-worklist.begin()][instr_number] = 1; // TODO add time value depending on instr type
#ifdef PRINT_DEBUG
              errs() << "\t   --> mem-dep - depends on " << it_pos-worklist.begin() << ": " << *(mdr.getInst()) << "\n";       
#endif     
            }
          }
        }

        // TODO what to do with 'ret void' ?
      }

#ifdef PRINT_DEBUG
      errs() << "\n" << "SSA dependency graph:" << "\n";
      for (std::vector< std::vector<unsigned int> >::iterator i = ssa_dep_graph.begin(); i != ssa_dep_graph.end(); ++i) {
        for (std::vector<unsigned int>::iterator j = i->begin(); j != i->end(); ++j) {
          errs() << *j << " ";
        }
        errs() << "\n";
      }
#endif

      std::vector<unsigned int> instr_times;
      for (int i=0; i<int(worklist.size()); i++)
        instr_times.push_back(1);

      for (std::vector< std::vector<unsigned int> >::iterator i = ssa_dep_graph.begin(); i != ssa_dep_graph.end(); ++i) {
        for (std::vector<unsigned int>::iterator j = i->begin(); j != i->end(); ++j) {
          if (*j > 0) {
            // there is an edge from node (i-ssa_dep_graph.begin()) to node (j-i->begin()) with weight *j
            instr_times[j-i->begin()] = std::max(instr_times[j-i->begin()], instr_times[i-ssa_dep_graph.begin()] + *j);
          }
        }
      }

#ifdef PRINT_DEBUG
      errs() << "\n" << "instruction times:" << "\n";
        errs() << "#instr" << "\t" << "delay" << "\t" << "instruction" << "\n";
        errs() << "------" << "\t" << "-----" << "\t" << "-----------" << "\n";
      for (std::vector<unsigned int>::iterator i = instr_times.begin(); i != instr_times.end(); ++i) 
        errs() << i-instr_times.begin() << "\t" << *i << "\t" << *worklist[i-instr_times.begin()] << "\n";
#endif
      unsigned int T_s = worklist.size(); // TODO add time value depending on type of the instructions
      unsigned int T_p = *std::max_element(instr_times.begin(), instr_times.end());
      errs() << "\n----------------------------\n";
      errs() << "runtime for computing all instructions sequentially: " << T_s << "\n"; 
      errs() << "runtime for computing instructions in parallel: " << T_p << "\n"; 
      errs() << "\n" << "speedup: " << float(T_s)/float(T_p) << "\n"; 
    return false;
  }

    // We don't modify the program, so we preserve all analyses.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequiredTransitive<MemoryDependenceAnalysis>();
      AU.setPreservesAll();
    }
  };
}

char MehariSpeedupAnalysis::ID = 0;
static RegisterPass<MehariSpeedupAnalysis>
Y("m-speedup", "Analysis of the possible speedup by instructions in parallel as much as possible.");
