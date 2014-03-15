#ifndef SPEEDUP_ANALYSIS_H_
#define SPEEDUP_ANALYSIS_H_

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include <boost/graph/adjacency_list.hpp>

using namespace llvm;

class SpeedupAnalysis : public FunctionPass {

public:
  static char ID;

  SpeedupAnalysis();
  ~SpeedupAnalysis();

  virtual bool runOnFunction(Function &func);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

private:
  std::vector<std::string> targetFunctions;
  void parseTargetFunctions();
  
	unsigned int getInstructionCost(Instruction *instruction);
	void buildDependencyGraph(InstructionDependencyList &dependencies);
	void printGraphviz();

	typedef boost::property<boost::edge_weight_t, int> EdgeWeightProperty;
	typedef boost::adjacency_list<boost::listS, boost::vecS, boost::bidirectionalS, boost::no_property, EdgeWeightProperty > Graph;
	//NOTE graph is just a simple directed graph, but it is declared bidirectional because the function in_edges requires this

	// the graph representing the dependencies between the instructions
	Graph depGraph;

	// a list containing all instructions of the function
	std::vector<Instruction*> worklist;
};

#endif /*SPEEDUP_ANALYSIS_H_*/
