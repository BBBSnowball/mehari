#ifndef IR_GRAPH_PRINTER_H
#define IR_GRAPH_PRINTER_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

using namespace llvm;

class IRGraphPrinter : public FunctionPass {

public:
	static char ID;

	IRGraphPrinter();
	~IRGraphPrinter();

	virtual bool runOnFunction(Function &func);
	virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	void printDataflowGraph(std::string &filename, Function &func);

private:
	std::vector<std::string> targetFunctions;
	void parseTargetFunctions();
};

#endif /*IR_GRAPH_PRINTER_H*/
