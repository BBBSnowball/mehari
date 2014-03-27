#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include <algorithm>

using namespace llvm;


static cl::opt<bool> Verbose ("v", cl::desc("Enable verbose output to get more information."));


InstructionDependencyAnalysis::InstructionDependencyAnalysis() : FunctionPass(ID) {
    initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
};

InstructionDependencyAnalysis::~InstructionDependencyAnalysis() {};


void InstructionDependencyAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequiredTransitive<MemoryDependenceAnalysis>();
	AU.setPreservesAll();
}


bool InstructionDependencyAnalysis::runOnFunction(Function &) {
  MDA = &getAnalysis<MemoryDependenceAnalysis>();
  return false;
}


InstructionDependencyList InstructionDependencyAnalysis::getDependencies(std::vector<Instruction*> &instructions) {

	InstructionDependencyList dependencies;

	// dependencies between instructions:
	//  - use-def chain: all instructions that are used by the current instruction -> data dependences on SSA registers
	//  - MemoryDependenceAnalysis: memory dependencies between instructions -> data dependences on memory locations

	// loop over all instructions and determine dependencies
	for (std::vector<Instruction*>::iterator instrIt = instructions.begin(); instrIt != instructions.end(); ++instrIt) {
  	Instruction *instr = dyn_cast<Instruction>(*instrIt);
  	
  	if (Verbose)
  		errs() << *instr << "\n";

  	InstructionDependencies currentDependencies;

		// evaluate use-def chain to get all instructions the current instruction depends on
		for (User::op_iterator opIt = instr->op_begin(); opIt != instr->op_end(); ++opIt) {
			Instruction *opInst = dyn_cast<Instruction>(*opIt);
			std::vector<Instruction*>::iterator itPos = std::find(instructions.begin(), instructions.end(), &*opInst);
			if (itPos != instructions.end()) {
				// add depedency
				currentDependencies.push_back(itPos-instructions.begin());
				if (Verbose)
					errs() << "\t   --> use-def - depends on " << itPos-instructions.begin() << ": " << *opInst << "\n";
		  	}
		}
		// if the current instruction is a load instruction, find the associated store instruction
		if (instr->mayReadFromMemory()) {
			MemDepResult mdr = MDA->getDependency(instr);
			if (mdr.isClobber()) {
		    std::vector<Instruction*>::iterator itPos = std::find(instructions.begin(), instructions.end(), &*mdr.getInst());
	    	if (itPos != instructions.end()) {
				// add depedency
				currentDependencies.push_back(itPos-instructions.begin());
				if (Verbose)
		   			errs() << "\t   --> mem-dep - depends on " << itPos-instructions.begin() << ": " << *(mdr.getInst()) << "\n";         
	    	}
		  }
		}
		// sort current dependencies
		std::sort(currentDependencies.begin(), currentDependencies.end());
		dependencies.push_back(currentDependencies);
	}
	return dependencies;
}

// register pass so we can call it using opt
char InstructionDependencyAnalysis::ID = 0;

INITIALIZE_PASS_BEGIN(InstructionDependencyAnalysis, "instrdep", 
	"Analysis of the dependencies between the instructions inside a function", true, true)
INITIALIZE_PASS_DEPENDENCY(MemoryDependenceAnalysis)
INITIALIZE_PASS_END(InstructionDependencyAnalysis, "instrdep", 
	"Analysis of the dependencies between the instructions inside a function", true, true)

// static RegisterPass<InstructionDependencyAnalysis>
// Y("instrdep", "Analysis of the dependencies between the instructions inside a function.");
