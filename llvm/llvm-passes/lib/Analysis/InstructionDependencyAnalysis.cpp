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
	AU.addRequiredTransitive<DependenceAnalysis>();
	AU.setPreservesAll();
}


bool InstructionDependencyAnalysis::runOnFunction(Function &) {
	DA = &getAnalysis<DependenceAnalysis>();
	return false;
}


InstructionDependencyList InstructionDependencyAnalysis::getDependencies(std::vector<Instruction*> &instructions) {

	InstructionDependencyList dependencies;

	// dependencies between instructions:
	//  - use-def chain: all instructions that are used by the current instruction -> data dependences on SSA registers
	//  - DependenceAnalysis: memory dependencies between instructions -> data dependences on memory locations

	// loop over all instructions and determine dependencies
	int instrCount = instructions.size();
	for (int i=0; i<instrCount; i++) {

		Instruction *instr = instructions[i];
	
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
					errs() << "USE-DEF: \t" << *instr << " depends on " << *opInst << "\n";
			}
		}
		
		// run the DependenceAnalysis on all instructions that access memory
		// to determine dependencies store->load
		if (instr->mayReadFromMemory() || instr->mayWriteToMemory()) {
			for (int j=0; j<i; j++) {
				Instruction *currentInstr = instructions[j];
				if (currentInstr->mayReadFromMemory() || currentInstr->mayWriteToMemory()) {
					Dependence *dep = DA->depends(currentInstr, instr, true);
					if (dep != NULL) {
						std::string srcOpcode = dep->getSrc()->getOpcodeName();
						std::string dstOpcode = dep->getDst()->getOpcodeName();						
						if (srcOpcode == "store" && dstOpcode == "load") 
							// compare operands for store->load operations
							if (dep->getSrc()->getOperand(1) == dep->getDst()->getOperand(0)) {
								// add depedency
								currentDependencies.push_back(j);
								if (Verbose)
									errs() << "MEM-DEP: " << *(dep->getDst()) << "  depends on  " << *(dep->getSrc()) << "\n";
							}
					}
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
INITIALIZE_PASS_DEPENDENCY(DependenceAnalysis)
INITIALIZE_PASS_END(InstructionDependencyAnalysis, "instrdep", 
	"Analysis of the dependencies between the instructions inside a function", true, true)

// static RegisterPass<InstructionDependencyAnalysis>
// Y("instrdep", "Analysis of the dependencies between the instructions inside a function.");
