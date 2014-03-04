#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include "llvm/Analysis/MemoryDependenceAnalysis.h"

#include <vector>

using namespace llvm;

// new type to represent the dependencies between instructions
// instruction #3 -> [instruction #1, instruction #2]: 
// instruction #3 depends on instruction #1 and instruction #2
typedef std::vector<unsigned int> InstructionDependencies;
typedef std::vector<InstructionDependencies> InstructionDependencyList;


class InstructionDependencyAnalysis : public FunctionPass {

public:
	static char ID;

	InstructionDependencyAnalysis();
	~InstructionDependencyAnalysis();

	virtual bool runOnFunction(Function &func);
	virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	InstructionDependencyList getDependencies(std::vector<Instruction*> &instructions);

private:
	MemoryDependenceAnalysis *MDA;
};
