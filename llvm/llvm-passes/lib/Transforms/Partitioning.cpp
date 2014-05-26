#include "mehari/Transforms/Partitioning.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"
#include "mehari/CodeGen/SimpleCCodeGenerator.h"
#include "mehari/CodeGen/TemplateWriter.h"

#include "llvm/IR/GlobalVariable.h"
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

	// init maximum numbers
	semNumberMax = 0;

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
		InstructionDependencyNumbersList dependencyNumbers = IDA->getDependencyNumbers(*func);

		// create partitioning graph
		PartitioningGraph *pGraph = new PartitioningGraph();
		pGraph->create(worklist, dependencyNumbers);
		
		// create partitioning
		applyRandomPartitioning(*pGraph, 42);

		// handle data and control dependencies between partitions
		// by adding appropriate function calls
		handleDependencies(M, *func, *pGraph, dependencies);
		
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


void Partitioning::handleDependencies(Module &M, Function &F, PartitioningGraph &pGraph, InstructionDependencyList &dependencies) {
	// create new functions to put or get data dependencies
	Function *newGetFloatFunc = cast<Function>(
		M.getOrInsertFunction("_get_real",
			Type::getDoubleTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));
	Function *newGetIntFunc = cast<Function>(
		M.getOrInsertFunction("_get_int",
			Type::getInt32Ty(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));
	Function *newGetBoolFunc = cast<Function>(
		M.getOrInsertFunction("_get_bool",
			Type::getInt1Ty(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));
	Function *newGetIntPtrFunc = cast<Function>(
		M.getOrInsertFunction("_get_intptr",
			Type::getInt32PtrTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));

	Function *newPutFloatFunc = cast<Function>(
		M.getOrInsertFunction("_put_real",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			Type::getDoubleTy(M.getContext()),
			(Type *)0));
	Function *newPutIntFunc = cast<Function>(
		M.getOrInsertFunction("_put_int",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));
	Function *newPutBoolFunc = cast<Function>(
		M.getOrInsertFunction("_put_bool",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			Type::getInt1Ty(M.getContext()),
			(Type *)0));
	Function *newPutIntPtrFunc = cast<Function>(
		M.getOrInsertFunction("_put_intptr",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			Type::getInt32PtrTy(M.getContext()),
			(Type *)0));

	// create new functions to handle semaphores
	Function *newSemWaitFunc = cast<Function>(
		M.getOrInsertFunction("_sem_wait",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()),
			(Type *)0));
	Function *newSemPostFunc = cast<Function>(
		M.getOrInsertFunction("_sem_post",
			Type::getVoidTy(M.getContext()),
			Type::getInt32Ty(M.getContext()), 
			(Type *)0));

	// reset counting of data dependencies and semaphore uses
	unsigned int depNumber = 0;
	unsigned int semNumber = 0;

	// loop over dependencies and insert appropriate function calls to handle dependencies between partitions
	for (InstructionDependencyList::iterator listIt = dependencies.begin(); listIt != dependencies.end(); ++listIt) {
		InstructionDependencyEntry depEntry = *listIt;
		Instruction *tgtinstr = depEntry.tgtInstruction;
		std::vector<InstructionDependency> instrDep = depEntry.dependencies;
		PartitioningGraph::VertexDescriptor instrVertex = pGraph.getVertexForInstruction(tgtinstr);
		if (instrVertex == NULL)
			// the instruction is not part of the Graph -> continue with the next instruction
			continue;
		for (std::vector<InstructionDependency>::iterator depValIt = instrDep.begin(); depValIt != instrDep.end(); ++depValIt) {
			Instruction *depInstr = depValIt->depInstruction;
			PartitioningGraph::VertexDescriptor depVertex = pGraph.getVertexForInstruction(depInstr);
			bool depNumberUsed = false;
			bool semNumberUsed = false;
			if (depVertex == NULL)
				// the dependency is not part of the Graph -> continue with the next instruction
				continue;
			if (pGraph.getPartition(instrVertex) != pGraph.getPartition(depVertex)) {				
				// create dependency and semaphore number
				Value *depNumberVal = ConstantInt::get(Type::getInt32Ty(M.getContext()), depNumber);
				Value *semNumberVal = ConstantInt::get(Type::getInt32Ty(M.getContext()), semNumber);

				// add function calls to handle dependencies between partitions
				std::vector<Instruction*> tgtInstrList = pGraph.getInstructions(instrVertex);
				std::vector<Instruction*>::iterator instrIt = std::find(tgtInstrList.begin(), tgtInstrList.end(), tgtinstr);
				Type *instrType = depInstr->getType();
				if (instrIt != tgtInstrList.end()) {
					CallInst *newInstr;
					if (depValIt->isMemDep || depValIt->isCtrlDep) {
						newInstr = CallInst::Create(newSemWaitFunc, semNumberVal);
						semNumberUsed = true;
					}
					else if (depValIt->isRegdep) {
						if (instrType->isIntegerTy()) {
							if (instrType->getIntegerBitWidth() == 1)
								newInstr = CallInst::Create(newGetBoolFunc, depNumberVal, "data");
							else
								newInstr = CallInst::Create(newGetIntFunc, depNumberVal, "data");
							dataDependencies.push_back("IntT");
						}
						else if (instrType->isFloatingPointTy()) {
							newInstr = CallInst::Create(newGetFloatFunc, depNumberVal, "data");
							dataDependencies.push_back("RealT");
						}
						else if (instrType->isPointerTy()) {
							newInstr = CallInst::Create(newGetIntPtrFunc, depNumberVal, "data");
							dataDependencies.push_back("IntT*");
						}						
						else {
							errs() 	<< "ERROR: unhandled type while using get_data: TypeID "
									<< instrType->getTypeID() << "\n";
							errs() << "Instruction: " << *depInstr << "\n";
							continue;
						}
						depNumberUsed = true;
					}
					// add metadata to specify the target operand for the get_data call
					std::stringstream ss;
					ss << depInstr;
					LLVMContext& context = tgtinstr->getContext();
					MDNode* mdn = MDNode::get(context, MDString::get(context, ss.str()));
					newInstr->setMetadata("targetop", mdn);
					// add instruction to function
					tgtinstr->getParent()->getInstList().insert(tgtinstr, newInstr);
					// add instruction to vertex instruction list
					tgtInstrList.insert(instrIt, newInstr);
					pGraph.setInstructions(instrVertex, tgtInstrList);
				}

				std::vector<Instruction*> depInstrList = pGraph.getInstructions(depVertex);
				std::vector<Instruction*>::iterator depIt = std::find(depInstrList.begin(), depInstrList.end(), depInstr);
				if (depIt != tgtInstrList.end()) {
					CallInst *newInstr;
					if (depValIt->isMemDep || depValIt->isCtrlDep) {
						newInstr = CallInst::Create(newSemPostFunc, semNumberVal);
						semNumberUsed = true;
					}
					else if (depValIt->isRegdep) {							
						std::vector<Value*> params;
						params.push_back(depNumberVal);
						params.push_back(depInstr);
						if (instrType->isIntegerTy()) {
							if (instrType->getIntegerBitWidth() == 1)
								newInstr = CallInst::Create(newPutBoolFunc, params);
							else
								newInstr = CallInst::Create(newPutIntFunc, params);
						}
						else if (instrType->isFloatingPointTy())
							newInstr = CallInst::Create(newPutFloatFunc, params);
						else if (instrType->isPointerTy())
							newInstr = CallInst::Create(newPutIntPtrFunc, params);
						else {
							errs() 	<< "ERROR: unhandled type while using put_data: TypeID "
									<< instrType->getTypeID() << "\n";
							errs() << "Instruction: " << *depInstr << "\n";
							continue;
						}
						depNumberUsed = true;
					}
					// add instruction to function
					depInstr->getParent()->getInstList().insertAfter(depInstr, newInstr);
					// add instruction to vertex instruction list
					depInstrList.insert(depIt+1, newInstr);
					pGraph.setInstructions(depVertex, depInstrList);
				}
				if (depNumberUsed)
					depNumber++;
				if (semNumberUsed)
					semNumber++;
			}
		}
	}
	// set maximum number of semaphore counts
	semNumberMax = std::max(semNumberMax, semNumber);
}


void Partitioning::savePartitioning(std::map<std::string, Function*> &functions, std::map<std::string, PartitioningGraph*> &graphs) {
	// set template and output files
	std::string mehariTemplate = TemplateDir + "/mehari.tpl";
	std::string threadDeclTemplate = TemplateDir + "/thread_declaration.tpl";
	std::string threadCreationTemplate = TemplateDir + "/thread_creation.tpl";
	std::string threadImplTemplate = TemplateDir + "/thread_implementation.tpl";
	std::string dataDepInitTemplate = TemplateDir + "/dep_data_init.tpl";
	std::string paramDepInitTemplate = TemplateDir + "/dep_param_init.tpl";
	std::string outputFile = OutputDir + "/mehari.c";

	// use the template write to fill template
	TemplateWriter tWriter;

	// use simple C code generator
	SimpleCCodeGenerator codeGen;

	// write extern global variables
	std::stringstream globVarOutput;
	for (std::vector<SimpleCCodeGenerator::GlobalArrayVariable>::iterator gvIt = globalVariables.begin(); gvIt != globalVariables.end(); ++gvIt)
		codeGen.createExternArray(globVarOutput, *gvIt);
	tWriter.setValue("GLOBAL_VARIABLES", globVarOutput.str());

	// write number of used semaphores
	std::string semDataCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << semNumberMax))->str();
	tWriter.setValue("SEM_DATA_COUNT", semDataCountStr);
	std::string semReturnCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << functions.size()))->str();
	tWriter.setValue("SEM_RETURN_COUNT", semReturnCountStr);

	// write number of data dependencies
	std::string dataDepCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << dataDependencies.size()))->str();
	tWriter.setValue("DATA_DEP_COUNT", dataDepCountStr);
	std::string dataDepParamCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << (functions.size()*(partitionCount-1))))->str();
	tWriter.setValue("DATA_DEP_PARAM_COUNT", dataDepParamCountStr);

	// insert initialization of data dependencies
	for (std::vector<std::string>::iterator depIt = dataDependencies.begin(); depIt != dataDependencies.end(); ++depIt) {
		std::string depNum = static_cast<std::ostringstream*>( &(std::ostringstream() << int(depIt-dataDependencies.begin())))->str();
		tWriter.setValueInSubTemplate(dataDepInitTemplate, "DEPENDENCY_INITIALIZATIONS",  depNum + "_DEP_INIT",
			"DEPENDENCY_NUMBER", depNum);
		tWriter.setValueInSubTemplate(dataDepInitTemplate, "DEPENDENCY_INITIALIZATIONS",  depNum + "_DEP_INIT",
			"DATA_TYPE", *depIt);
	}

	// insert partition and thread count
	std::string partitionCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << partitionCount))->str();
	tWriter.setValue("PARTITION_COUNT", partitionCountStr);
	unsigned int threadCount = functions.size()*(partitionCount-1);
	std::string threadCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << threadCount))->str();
	tWriter.setValue("THREAD_COUNT", threadCountStr);

	// count threads during function writing
	unsigned int threadNumber = 0;

	// write partitioning for each function
	unsigned int functionIndex = 0;
	for (std::map<std::string, Function*>::iterator funcIt = functions.begin(); funcIt != functions.end(); ++funcIt, functionIndex++) {
		std::string currentFunction = funcIt->first;
		std::string currentFunctionUppercase = boost::to_upper_copy(currentFunction);
		std::string functionIndexStr = static_cast<std::ostringstream*>( &(std::ostringstream() << functionIndex))->str();

		std::string functionTemplate = TemplateDir + "/" + currentFunction + "_func.tpl";
		std::string funcCallTemplate = TemplateDir + "/" + currentFunction + "_call.tpl";

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
			std::string partitionNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << i))->str();
			std::string functionName = currentFunction + "_" + partitionNumber;
			std::string functionBody = codeGen.createCCode(*func, instructionsForPartition[i]);
			// create a new thread for the evaluation functions 1+
			if (i > 0) {
				// add thread declaration
				tWriter.setValueInSubTemplate(threadDeclTemplate, "THREAD_DECLARATIONS", functionName + "_THREAD_DECL",
					"FUNCTION_NAME", functionName);
				// add thread creation
				std::string threadNumberStr = static_cast<std::ostringstream*>( &(std::ostringstream() << threadNumber))->str();
				tWriter.setValueInSubTemplate(threadCreationTemplate, "THREAD_CREATIONS", functionName + "_THREAD_CREATION",
					"FUNCTION_NAME", functionName);
				tWriter.setValueInSubTemplate(threadCreationTemplate, "THREAD_CREATIONS", functionName + "_THREAD_CREATION",
					"THREAD_NUMBER", threadNumberStr);
				// add thread implementation
				tWriter.setValueInSubTemplate(threadImplTemplate, currentFunctionUppercase + "_THREADS", functionName + "_THREADS",
					"FUNCTION_NAME", functionName);
				tWriter.setValueInSubTemplate(threadImplTemplate, currentFunctionUppercase + "_THREADS", functionName + "_THREADS",
					"FUNCTION", currentFunction);
				tWriter.setValueInSubTemplate(threadImplTemplate, currentFunctionUppercase + "_THREADS", functionName + "_THREADS",
					"THREAD_NUMBER", threadNumberStr);
				// NOTE: this call sets a sub-template in a sub-template!
				tWriter.setValueInSubTemplate(funcCallTemplate, "FUNCTION_CALL", functionName + "_THREADS_CALL",
					"FUNCTION_NUMBER", partitionNumber, functionName + "_THREADS");
				
				// increment thread number for next thread creation
				threadNumber++;
			}
			// create new function
			tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", functionName + "_FUNCTIONS",
				"FUNCTION_NUMBER", partitionNumber);
			tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", functionName + "_FUNCTIONS",
				"FUNCTION_BODY", functionBody);
			tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", functionName + "_FUNCTIONS",
				"RETURN_SEM_INDEX", functionIndexStr);

			// add implementation to main calculation function
			unsigned int putParamStart = (partitionCount-1) * functionIndex;
			std::string putParamStartStr = static_cast<std::ostringstream*>( &(std::ostringstream() << putParamStart))->str();
			tWriter.setValue(currentFunctionUppercase + "_PUT_PARAM_START",  putParamStartStr);
			tWriter.setValue(currentFunctionUppercase + "_RETURN_SEM_INDEX", functionIndexStr);

			// add initialization of parameter depdendencies
			for (int j=putParamStart; j<putParamStart+partitionCount-1; j++) {
				std::string depNum = static_cast<std::ostringstream*>( &(std::ostringstream() << j))->str();
				tWriter.setValueInSubTemplate(paramDepInitTemplate, "DEPENDENCY_INITIALIZATIONS",  depNum + "_DEP_PARAM_INIT",
					"DEPENDENCY_NUMBER", depNum);
				tWriter.setValueInSubTemplate(paramDepInitTemplate, "DEPENDENCY_INITIALIZATIONS",  depNum + "_DEP_PARAM_INIT",
					"FUNCTION_NAME", currentFunction);	
			}
		}
	}

	// expand and save template
	tWriter.expandTemplate(mehariTemplate);
	tWriter.writeToFile(outputFile);
}


// register pass so we can call it using opt
char Partitioning::ID = 0;
static RegisterPass<Partitioning>
Y("partitioning", "Create a partitioning for the calculation inside the function.");
