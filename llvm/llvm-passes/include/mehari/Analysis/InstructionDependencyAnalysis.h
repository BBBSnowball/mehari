#ifndef INSTRUCTION_DEPENDENCY_ANALYSIS_H_
#define INSTRUCTION_DEPENDENCY_ANALYSIS_H_


#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include "llvm/Analysis/DependenceAnalysis.h"

#include <vector>

using namespace llvm;

namespace llvm {
	void initializeInstructionDependencyAnalysisPass(PassRegistry&);
}

// new type to represent the dependencies between instructions
// instruction #3 -> [instruction #1, instruction #2]: 
// instruction #3 depends on instruction #1 and instruction #2
typedef std::vector<unsigned int> InstructionDependencyNumbers;
typedef std::vector<InstructionDependencyNumbers> InstructionDependencyNumbersList;

// new representation of dependencies storing the real instructions
// and some more logic instead of just the instruction numbers
typedef struct InstructionDependency {
	InstructionDependency() : isRegdep(false), isMemDep(false), isCtrlDep(false) {}
	Instruction *depInstruction;
	bool isRegdep;
	bool isMemDep;
	bool isCtrlDep;
} InstructionDependency;

typedef struct InstructionDependencyEntry {
	Instruction *tgtInstruction;
	std::vector<InstructionDependency> dependencies;
} InstructionDependencyEntry;

typedef std::vector<InstructionDependencyEntry> InstructionDependencyList;


class InstructionDependencyAnalysis : public FunctionPass {

public:
	static char ID;

	InstructionDependencyAnalysis();
	~InstructionDependencyAnalysis();

	virtual bool runOnFunction(Function &func);
	virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	InstructionDependencyList getDependencies(Function &func);
	InstructionDependencyNumbersList getDependencyNumbers(Function &func);

private:
	DependenceAnalysis *DA;

	int getInstructionNumber(std::vector<Instruction*> &instructionList, Instruction* instruction);
};

#endif /*INSTRUCTION_DEPENDENCY_ANALYSIS_H_*/
