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
typedef std::vector<unsigned int> InstructionDependencies;
typedef std::vector<InstructionDependencies> InstructionDependencyList;

// new representation of dependencies storing the real instructions
// instead of just the numbers
typedef std::vector<Instruction*> InstructionDependencyValues;
typedef struct {
	Instruction *instruction;
	InstructionDependencyValues *dependencies;
} InstructionDependencyValueListEntry;
typedef std::vector<InstructionDependencyValueListEntry> InstructionDependencyValueList;


class InstructionDependencyAnalysis : public FunctionPass {

public:
	static char ID;

	InstructionDependencyAnalysis();
	~InstructionDependencyAnalysis();

	virtual bool runOnFunction(Function &func);
	virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	InstructionDependencyList getDependencies(Function &func);
	InstructionDependencyValueList getDependencyValues(Function &func);


private:
	DependenceAnalysis *DA;

	int getInstructionNumber(std::vector<Instruction*> &instructionList, Instruction* instruction);
};

#endif /*INSTRUCTION_DEPENDENCY_ANALYSIS_H_*/
