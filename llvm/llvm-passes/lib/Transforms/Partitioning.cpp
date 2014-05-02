#include "mehari/Transforms/Partitioning.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"
#include "mehari/CodeGen/SimpleCCodeGenerator.h"
#include "mehari/CodeGen/TemplateWriter.h"

#include "llvm/IR/GlobalVariable.h"

//DEBUG
#include "llvm/IR/Constants.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>


static cl::opt<std::string> TargetFunctions("partitioning-functions", 
            cl::desc("Specify the functions the partitioning will be applyed on (seperated by whitespace)"), 
            cl::value_desc("target-functions"));
static cl::opt<std::string> PartitionCount("partitioning-count", 
            cl::desc("Specify the number ob partitions"), 
            cl::value_desc("partitioning-count"));
static cl::opt<std::string> OutputDir("partitioning-output-dir", 
            cl::desc("Set the output directory for graph results"), 
            cl::value_desc("partitioning-output-dir"));
static cl::opt<std::string> TemplateDir("template-dir", 
            cl::desc("Set the directory where the code generation templates are located"), 
            cl::value_desc("template-dir"));


Partitioning::Partitioning() : ModulePass(ID) {
	initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
	// read command line arguments
	parseTargetFunctions();
	std::stringstream ss(PartitionCount);
	ss >> partitionCount;
}

Partitioning::~Partitioning() {}


bool Partitioning::runOnModule(Module &M) {

	// read global variables
	for (Module::global_iterator globIt = M.global_begin(); globIt != M.global_end(); ++globIt) {
		GlobalVariable *gv = dyn_cast<GlobalVariable>(globIt);
		SimpleCCodeGenerator::GlobalArrayVariable globVar;
		if (!gv->isConstant() && gv->getType()->isPointerTy()) {
			Type *pointerType = gv->getType()->getPointerElementType();
			if (pointerType->isArrayTy()) {
				Type *arrayElemType = pointerType->getArrayElementType();
				if (arrayElemType->isIntegerTy() || arrayElemType->isFloatingPointTy()) {
					globVar.name = gv->getName().str();
					globVar.numElem = pointerType->getArrayNumElements();
					if (arrayElemType->isIntegerTy())
						globVar.type = "int";
					else if (arrayElemType->isFloatingPointTy())
						globVar.type = "double";
					globalVariables.push_back(globVar);
				}
			}
		}
	}

	// create partitioning for each target function
	std::map<std::string, Function*> partitioningFunctions;
	std::map<std::string, PartitioningGraph*> partitioningGraphs;

	for (std::vector<std::string>::iterator funcIt = targetFunctions.begin(); funcIt != targetFunctions.end(); ++funcIt) {
		Function *func = M.getFunction(*funcIt);

		if (!func) {
			errs() << "ERROR: Function " << *funcIt << " not found!\n";
			break;
		}

		std::string functionName = func->getName().str();

		errs() << "\npartitioning: " << functionName << "\n\n";

		// create worklist containing all instructions of the function
		std::vector<Instruction*> worklist;
		for (inst_iterator I = inst_begin(*func), E = inst_end(*func); I != E; ++I)
			worklist.push_back(&*I);

		// run InstructionDependencyAnalysis
		InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>(*func);
		InstructionDependencyList dependencies = IDA->getDependencies(*func);
		InstructionDependencyValueList depencencyValues = IDA->getDependencyValues(*func);

		// create partitioning graph
		PartitioningGraph *pGraph = new PartitioningGraph();
		pGraph->create(worklist, dependencies);
		
		// create partitioning
		applyRandomPartitioning(*pGraph, 42);

		// handle data and control dependencies between partitions
		// by adding appropriate function calls
		handleDependencies(M, *func, *pGraph, depencencyValues);
		
		// print partitioning graph results
		// pGraph->printGraph(functionName);
		pGraph->printGraphviz(*func, functionName, OutputDir);

		// save partitioning function and graph
		partitioningFunctions[functionName] = func;
		partitioningGraphs[functionName] = pGraph;
	}

	// print partitioning results into template
	savePartitioning(partitioningFunctions, partitioningGraphs);

	return false;
}


void Partitioning::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<InstructionDependencyAnalysis>();
	AU.setPreservesAll();
}


void Partitioning::parseTargetFunctions(void) {
  boost::algorithm::split(targetFunctions, TargetFunctions, boost::algorithm::is_any_of(" "));
}


void Partitioning::applyRandomPartitioning(PartitioningGraph &pGraph, unsigned int seed) {
	srand(seed);
	PartitioningGraph::VertexIterator vIt = pGraph.getFirstIterator(); 
	PartitioningGraph::VertexIterator vEnd = pGraph.getEndIterator();
	for (; vIt != vEnd; ++vIt) {
		PartitioningGraph::VertexDescriptor vd = *vIt;
		pGraph.setPartition(vd, rand()%partitionCount);
	}
}


void Partitioning::handleDependencies(Module &M, Function &F, PartitioningGraph &pGraph, InstructionDependencyValueList &dependencies) {
	// create new functions to put or get data dependencies
	Function *newGetFunc = cast<Function>(
		M.getOrInsertFunction("get_data",
			Type::getDoubleTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));
	Function *newPutFunc = cast<Function>(
		M.getOrInsertFunction("put_data",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()), 
			(Type *)0));
	// TODO: add paremeter for put value -> put_data(depNumber, data)

	// enumerate the dependencies
	unsigned int depNumber = 1;

	// loop over dependencies and insert appropriate function calls to handle dependencies between partitions
	for (InstructionDependencyValueList::iterator listIt = dependencies.begin(); listIt != dependencies.end(); ++listIt) {
		InstructionDependencyValueListEntry depEntry = *listIt;
		Instruction *instr = depEntry.instruction;
		InstructionDependencyValues *instrDep = depEntry.dependencies;
		PartitioningGraph::VertexDescriptor instrVertex = pGraph.getVertexForInstruction(instr);
		if (instrVertex == NULL)
			// the instruction is not part of the Graph -> continue with the next instruction
			continue;
		for (InstructionDependencyValues::iterator depValIt = instrDep->begin(); depValIt != instrDep->end(); ++depValIt) {
			PartitioningGraph::VertexDescriptor depVertex = pGraph.getVertexForInstruction(*depValIt);
			if (depVertex == NULL)
				// the dependency is not part of the Graph -> continue with the next instruction
				continue;
			if (pGraph.getPartition(instrVertex) != pGraph.getPartition(depVertex)) {
				// errs() << "Instruction and dependency are in different partitions:\n";
				// errs() << "instr: (" << pGraph.getPartition(instrVertex) << ") - " << *instr  << "\n";
				// errs() << "dep:   (" << pGraph.getPartition(depVertex)   << ") - " << **depValIt << "\n";
				// errs() << "\n";
				
				// create dependency number
				Value *number = ConstantInt::get(Type::getInt32Ty(M.getContext()), depNumber);

				// add function calls to handle dependencies between partitions
				std::vector<Instruction*> tgtInstrList = pGraph.getInstructions(instrVertex);
				std::vector<Instruction*>::iterator instrIt = std::find(tgtInstrList.begin(), tgtInstrList.end(), instr);
				if (instrIt != tgtInstrList.end()) {
					CallInst *newInstr = CallInst::Create(newGetFunc, number, "data");
					// add instruction to function
					instr->getParent()->getInstList().insert(instr, newInstr);
					// add instruction to vertex instruction list
					tgtInstrList.insert(instrIt, newInstr);
					pGraph.setInstructions(instrVertex, tgtInstrList);
					// TODO: change operand in target instruction to use the new temporary variable that contains the data
					// target operand == dependency !?
					// for (User::op_iterator opIt = instr->op_begin(); opIt != instr->op_end(); ++opIt) {
					// 	errs() << "   * " << **opIt << "\n";
					// }
				}

				std::vector<Instruction*> depInstrList = pGraph.getInstructions(depVertex);
				std::vector<Instruction*>::iterator depIt = std::find(depInstrList.begin(), depInstrList.end(), *depValIt);
				if (depIt != tgtInstrList.end()) {
					CallInst *newInstr = CallInst::Create(newPutFunc, number, "");
					// add instruction to function
					(*depValIt)->getParent()->getInstList().insert(*depValIt, newInstr); // TODO: add put instr behind instruction!
					// add instruction to vertex instruction list
					depInstrList.insert(depIt+1, newInstr);
					pGraph.setInstructions(depVertex, depInstrList);
				}

				depNumber++;
			}
		}
	}
}


void Partitioning::savePartitioning(std::map<std::string, Function*> &functions, std::map<std::string, PartitioningGraph*> &graphs) {
	// set template and output files
	std::string mehariTemplate = TemplateDir + "/mehari.tpl";
	std::string threadDeclTemplate = TemplateDir + "/thread_declaration.tpl";
	std::string threadImplTemplate = TemplateDir + "/thread_implementation.tpl";
	std::string outputFile = OutputDir + "/mehari.c";

	// use the template write to fill template
	TemplateWriter tWriter;

	// use simple C code generator
	SimpleCCodeGenerator codeGen;

	// write extern global variables
	std::string globVarOutput;
	for (std::vector<SimpleCCodeGenerator::GlobalArrayVariable>::iterator gvIt = globalVariables.begin(); gvIt != globalVariables.end(); ++gvIt)
		globVarOutput += codeGen.createExternArray(*gvIt);
	tWriter.setValue("GLOBAL_VARIABLES", globVarOutput);

	// write partitioning for each function
	for (std::map<std::string, Function*>::iterator funcIt = functions.begin(); funcIt != functions.end(); ++funcIt) {
		std::string currentFunction = funcIt->first;
		std::string currentFunctionUppercase = boost::to_upper_copy(currentFunction);

		std::string functionTemplate = TemplateDir + "/" + currentFunction + "_func.tpl";

		Function *func = funcIt->second;
		PartitioningGraph *pGraph = graphs[currentFunction];

		// enable the section for this function
		tWriter.enableSection(currentFunctionUppercase + "_IMPLEMENTATION");

		// collect the instructions for each partition
		std::vector<Instruction*> instructionsForPartition[partitionCount];
		PartitioningGraph::VertexIterator vIt = pGraph->getFirstIterator(); 
		PartitioningGraph::VertexIterator vEnd = pGraph->getEndIterator();
		for (; vIt != vEnd; ++vIt) {
			PartitioningGraph::VertexDescriptor vd = *vIt;
			unsigned int partitionNumber = pGraph->getPartition(vd);
			std::vector<Instruction*> instructions = pGraph->getInstructions(vd);
			for (std::vector<Instruction*>::iterator it = instructions.begin(); it != instructions.end(); ++it) {
				Instruction *instr = dyn_cast<Instruction>(*it);
				instructionsForPartition[partitionNumber].push_back(instr);
			}
		}

		// generate C code for each partition of this function and write it into template
		for (int i=0; i<partitionCount; i++) {
			std::string partitionNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << i) )->str();
			std::string functionName = currentFunction + "_" + partitionNumber;
			std::string functionBody = codeGen.createCCode(*func, instructionsForPartition[i]);
			// create a new thread for the evaluation functions 1+
			if (i > 0) {
				// add declaration
				tWriter.setValueInSubTemplate(threadDeclTemplate, "THREAD_DECLARATIONS", partitionNumber,
					"FUNCTION_NAME", functionName);
				// add implementation
				tWriter.setValueInSubTemplate(threadImplTemplate, currentFunctionUppercase + "_THREADS", partitionNumber,
					"FUNCTION_NAME", functionName);
				tWriter.setValueInSubTemplate(threadImplTemplate, currentFunctionUppercase + "_THREADS", partitionNumber,
					"FUNCTION", currentFunction);
			}
			// create new function
			tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", partitionNumber, 
				"FUNCTION_NAME", functionName);
			tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", partitionNumber, 
				"FUNCTION_BODY", functionBody);

			// TODO: add implementation of main evaluation function
		}
	}

	// TODO: set values depending on the partitioning results
	tWriter.setValue("SEMAPHORE_COUNT", "5");
	tWriter.setValue("DATA_DEP_COUNT", "8");

	// insert thread count
	std::string threadCount = static_cast<std::ostringstream*>( &(std::ostringstream() << (partitionCount-1)) )->str();
	tWriter.setValue("THREAD_COUNT", threadCount);

	// expand and save template
	tWriter.expandTemplate(mehariTemplate);
	tWriter.writeToFile(outputFile);
}


// register pass so we can call it using opt
char Partitioning::ID = 0;
static RegisterPass<Partitioning>
Y("partitioning", "Create a partitioning for the calculation inside the function.");
