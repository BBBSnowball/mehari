#include "mehari/Transforms/Partitioning.h"
#include "mehari/Transforms/PartitioningAlgorithms.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"
#include "mehari/CodeGen/SimpleCCodeGenerator.h"
#include "mehari/CodeGen/TemplateWriter.h"
#include "mehari/CodeGen/GenerateVHDL.h"

#include "mehari/utils/StringUtils.h"

#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/CommandLine.h"

#include <sstream>
#include <fstream>
#include <ctime>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/foreach.hpp>
#include <boost/assign.hpp>


static cl::opt<std::string> TargetFunctions("partitioning-functions", 
            cl::desc("Specify the functions the partitioning will be applyed on (seperated by whitespace)"), 
            cl::value_desc("target-functions"));
static cl::opt<std::string> PartitioningMethods("partitioning-methods", 
            cl::desc("Specify the name of the partitioning methods"), 
            cl::value_desc("partitioning-methods"));
static cl::opt<std::string> PartitionDevices("partitioning-devices", 
            cl::desc("Specify the devices to execute the partitions"), 
            cl::value_desc("partitioning-devices"));
static cl::opt<std::string> OutputDir("partitioning-output-dir", 
            cl::desc("Set the output directory for partitioning results"), 
            cl::value_desc("partitioning-output-dir"));
static cl::opt<std::string> GraphOutputDir("partitioning-graph-output-dir", 
            cl::desc("Set the output directory for graph results"),
            cl::value_desc("partitioning-output-dir"));
static cl::opt<std::string> TemplateDir("template-dir", 
            cl::desc("Set the directory where the code generation templates are located"), 
            cl::value_desc("template-dir"));


// Use data dependencies for all communication with the FPGA
static bool useDataDepForAllFPGACom = false;


Partitioning::Partitioning() : ModulePass(ID) {
	initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
	// read command line arguments
	parseTargetFunctions();
	parsePartitioningMethods();
	parsePartitioningDevices();
	// init dependency number to count them over all partitioned functions
	depNumber = 0;
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
	std::map<std::string, unsigned int> partitioningNumbers;

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

		// create partitioning graph
		PartitioningGraph *pGraph = new PartitioningGraph();
		pGraph->create(worklist, dependencies);
		
		// create partitioning
		std::string methodsListString;
		for (std::vector<std::string>::iterator it = partitioningMethods.begin(); it != partitioningMethods.end(); ++it) {
			std::string pMethod = *it;
			methodsListString += " " + pMethod;
			// if the first algorithm that should be executed is an iterative algorithm that needs a starting point,
			// create an random partitioning before
			if ((it-partitioningMethods.begin() == 0) && (pMethod == "sa" || pMethod == "k-lin")) {
				RandomPartitioning rPM;
				if (pMethod == "k-lin")
					rPM.balancedBiPartitioning(*pGraph);
				else
					rPM.apply(*pGraph, partitioningDevices);
			}

			if (pMethod == "nop") {
				partitioningNumbers[functionName] = 1;
			}
			else {
				AbstractPartitioningMethod *PM;
				
				if (pMethod == "random")
					PM = new RandomPartitioning();
				else if (pMethod == "clustering")
					PM = new HierarchicalClustering();
				else if (pMethod == "sa")
					PM = new SimulatedAnnealing();
				else if (pMethod == "k-lin")
					PM = new KernighanLin();
				else
					throw std::runtime_error("Invalid partitioning method!");

				clock_t start = std::clock();
				partitioningNumbers[functionName] = PM->apply(*pGraph, partitioningDevices);
				clock_t ends = std::clock();
				delete PM;

				double runtime = (double) (ends - start) / CLOCKS_PER_SEC * 1000;
				errs() << "Runtime for partitioning " << functionName << " using " << pMethod << ": " 
					<< format("%4.4f", runtime) << " ms\n";
			}

			// print partitioning graph results
			pGraph->printGraphviz(*func, functionName + "_" + pMethod, GraphOutputDir);
		}

		// print critical path of partitioning graph to evaluate the partitioning result
		std::ofstream criticalPathFile;
		std::string criticalPathFileName = OutputDir + "/critical_path.txt";
		criticalPathFile.open(criticalPathFileName.c_str());
		criticalPathFile << "Critical path length for using [" << methodsListString
		<< " ]: " << pGraph->getCriticalPathCost(partitioningDevices) << "\n";
		criticalPathFile.close();

		// handle data and control dependencies between partitions
		// by adding appropriate function calls
		handleDependencies(M, *func, *pGraph, dependencies);

		// save partitioning function and graph
		partitioningFunctions[functionName] = func;
		partitioningGraphs[functionName] = pGraph;
	}

	// print partitioning results into template
	savePartitioning(partitioningFunctions, partitioningGraphs, partitioningNumbers);

	return false;
}


void Partitioning::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<InstructionDependencyAnalysis>();
	AU.setPreservesAll();
}


void Partitioning::parseTargetFunctions(void) {
  boost::algorithm::split(targetFunctions, TargetFunctions, boost::algorithm::is_any_of(" "));
}


void Partitioning::parsePartitioningMethods(void) {
  boost::algorithm::split(partitioningMethods, PartitioningMethods, boost::algorithm::is_any_of("+"));
}


void Partitioning::parsePartitioningDevices(void) {
  boost::algorithm::split(partitioningDevices, PartitionDevices, boost::algorithm::is_any_of(" "));
  partitionCount = partitioningDevices.size();
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

	// reset counting semaphore uses (semaphores are reused in each partitioned function)
	// start counting at one, because semaphore 0 is used for return control
	unsigned int semNumber = 1;

	// loop over dependencies and insert appropriate function calls to handle dependencies between partitions
	for (InstructionDependencyList::iterator listIt = dependencies.begin(); listIt != dependencies.end(); ++listIt) {
		InstructionDependencyEntry depEntry = *listIt;
		Instruction *tgtInstr = depEntry.tgtInstruction;
		std::vector<InstructionDependency> instrDep = depEntry.dependencies;
		PartitioningGraph::VertexDescriptor instrVertex = pGraph.getVertexForInstruction(tgtInstr);
		if (instrVertex == NO_SUCH_VERTEX)
			// the instruction is not part of the Graph -> continue with the next instruction
			continue;
		for (std::vector<InstructionDependency>::iterator depValIt = instrDep.begin(); depValIt != instrDep.end(); ++depValIt) {
			Instruction *depInstr = depValIt->depInstruction;
			Type *instrType = depInstr->getType();
			PartitioningGraph::VertexDescriptor depVertex = pGraph.getVertexForInstruction(depInstr);
			bool depNumberUsed = false;
			bool semNumberUsed = false;
			if (depVertex == NO_SUCH_VERTEX)
				// the dependency is not part of the Graph -> continue with the next instruction
				continue;
			if (pGraph.getPartition(instrVertex) != pGraph.getPartition(depVertex)) {				
				// create dependency and semaphore number
				Value *depNumberVal = ConstantInt::get(Type::getInt32Ty(M.getContext()), depNumber);
				Value *semNumberVal = ConstantInt::get(Type::getInt32Ty(M.getContext()), semNumber);

				// determine partition hardware types
				HardwareInformation hInfo;
				DeviceInformation::DeviceType t1, t2;
				t1 =hInfo.getDeviceInfo(
					partitioningDevices[pGraph.getPartition(instrVertex)])->getType();
				t2 =hInfo.getDeviceInfo(
					partitioningDevices[pGraph.getPartition(depVertex)])->getType();

				// determine whether to use mboxes or semaphores
				// currently we do not use semaphores for the communication with the FPGA, only for control flow
				// so we use data dependency methods every time we communicate with the FPGA
				// or we communicate between two processor cores and the current dependency is a register dependency
				bool useSemaphores;
				if (useDataDepForAllFPGACom)
					useSemaphores = depValIt->isCtrlDep
					|| (t1 != DeviceInformation::FPGA_RECONOS && t2 != DeviceInformation::FPGA_RECONOS && depValIt->isMemDep);
				else
					useSemaphores = depValIt->isCtrlDep || depValIt->isMemDep;

				// determine whether we handle a calculation instruction (return type int/real) or a load/store instruction (void)
				bool isVoidInstr = instrType->isVoidTy();
				Type *operandType = NULL;
				if (isVoidInstr)
					operandType = depInstr->getOperand(0)->getType();

				// add function calls to handle dependencies between partitions
				std::vector<Instruction*> tgtInstrList = pGraph.getInstructions(instrVertex);
				std::vector<Instruction*>::iterator instrIt = std::find(tgtInstrList.begin(), tgtInstrList.end(), tgtInstr);
				if (instrIt != tgtInstrList.end()) {
					CallInst *newInstr;
					// create the new function depending on the target hardware and the communication type
					if (useSemaphores) {
						newInstr = CallInst::Create(newSemWaitFunc, semNumberVal);
						semNumberUsed = true;
					}
					else { // we use data dependencies
						if (instrType->isIntegerTy() || (isVoidInstr && operandType->isIntegerTy())) {
							if (instrType->getIntegerBitWidth() == 1)
								newInstr = CallInst::Create(newGetBoolFunc, depNumberVal, "data");
							else
								newInstr = CallInst::Create(newGetIntFunc, depNumberVal, "data");
							dataDependencies.push_back("IntT");
						}
						else if (instrType->isFloatingPointTy() || (isVoidInstr && operandType->isFloatingPointTy())) {
							newInstr = CallInst::Create(newGetFloatFunc, depNumberVal, "data");
							dataDependencies.push_back("RealT");
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
					// for load/store add the operand to the metadata, else we can use the instruction itself
					std::stringstream ss;
					if(isVoidInstr)
						ss << tgtInstr;
					else
						ss << depInstr;
					LLVMContext &context = tgtInstr->getContext();
					MDNode* mdn = MDNode::get(context, MDString::get(context, ss.str()));
					newInstr->setMetadata("targetop", mdn);
					
					// insert instruction: if we are inside an if statement we should insert the get method 
					// at the beginning of the statement, otherwise we can insert it directly before the target instruction
					std::vector<Instruction*>::iterator insertTarget = instrIt;
					bool containsBranch = false;
					for (std::vector<Instruction*>::iterator it = tgtInstrList.begin(); it != tgtInstrList.end(); ++it)
						if (isa<BranchInst>(*it)) 
							containsBranch = true;
					if (containsBranch)
						insertTarget = tgtInstrList.begin();

					// add instruction to function
					Instruction *tgt = *insertTarget;
					tgt->getParent()->getInstList().insert(tgt, newInstr);
					// add instruction to vertex instruction list
					tgtInstrList.insert(insertTarget, newInstr);
					pGraph.setInstructions(instrVertex, tgtInstrList);
				}

				std::vector<Instruction*> depInstrList = pGraph.getInstructions(depVertex);
				std::vector<Instruction*>::iterator depIt = std::find(depInstrList.begin(), depInstrList.end(), depInstr);
				if (depIt != tgtInstrList.end()) {
					CallInst *newInstr;
					if (useSemaphores) {
						newInstr = CallInst::Create(newSemPostFunc, semNumberVal);
						semNumberUsed = true;
					}
					else { // we use data dependencies
						std::vector<Value*> params;
						params.push_back(depNumberVal);
						// for load/store add the operand to the parameter list, else we can use the instruction itself
						if (isVoidInstr)
							params.push_back(depInstr->getOperand(0));
						else
							params.push_back(depInstr);
						if (instrType->isIntegerTy() || (isVoidInstr && operandType->isIntegerTy())) {
							if (instrType->getIntegerBitWidth() == 1)
								newInstr = CallInst::Create(newPutBoolFunc, params);
							else
								newInstr = CallInst::Create(newPutIntFunc, params);
						}
						else if (instrType->isFloatingPointTy() || (isVoidInstr && operandType->isFloatingPointTy())) {
							newInstr = CallInst::Create(newPutFloatFunc, params);
						}
						else {
							errs() 	<< "ERROR: unhandled type while using put_data: TypeID "
									<< instrType->getTypeID() << "\n";
							errs() << "Instruction: " << *depInstr << "\n";
							continue;
						}
						depNumberUsed = true;
					}
					
					// insert instruction: if we are inside an if statement we should insert the put method 
					// at the end of the statement, otherwise we can insert it directly after the target instruction
					std::vector<Instruction*>::iterator insertTarget = depIt;
					bool containsBranch = false;
					for (std::vector<Instruction*>::iterator it = depInstrList.begin(); it != depInstrList.end(); ++it)
						if (isa<BranchInst>(*it)) 
							containsBranch = true;
					if (containsBranch)
						insertTarget = depInstrList.end()-1;

					// add instruction to function
					Instruction *tgt = *insertTarget;
					depInstr->getParent()->getInstList().insertAfter(tgt, newInstr);
					// add instruction to vertex instruction list
					depInstrList.insert(insertTarget+1, newInstr);
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


void saveOperator(const std::string& filename, ::Operator* op, const std::string& operatorName) {
	if (!operatorName.empty())
		op->setName(operatorName);

	std::ofstream file;
	file.open(filename.c_str(), std::ios::out);
	op->outputVHDL(file, (operatorName.empty() ? op->getName() : operatorName));
	file.close();
}


struct GenerateHardwareThreadFileFromTemplate {
	std::string TemplateDir, hardwareThreadDir, hardwareThreadName, hardwareThreadVersion;

	GenerateHardwareThreadFileFromTemplate& generate(const std::string& templateFile, const std::string& filename) {
		TemplateWriter templateWriter;
		templateWriter.setValue("HWT_NAME",    hardwareThreadName);
		templateWriter.setValue("HWT_VERSION", hardwareThreadVersion);

		templateWriter.expandTemplate(TemplateDir + "/" + templateFile);
		templateWriter.writeToFile(hardwareThreadDir + "/" + filename);

		return *this;
	}
};


void Partitioning::savePartitioning(std::map<std::string, Function*> &functions, 
	std::map<std::string, PartitioningGraph*> &graphs, std::map<std::string, unsigned int> partitioningNumbers) {
	// set template and output files
	std::string mehariTemplate = TemplateDir + "/mehari.tpl";
	std::string threadDeclTemplate = TemplateDir + "/thread_declaration.tpl";
	std::string threadCreationTemplate = TemplateDir + "/thread_creation.tpl";
	std::string threadImplTemplate = TemplateDir + "/thread_implementation.tpl";
	std::string partitionCountTemplate = TemplateDir + "/partition_count_entry.tpl";
	std::string dataDepInitTemplate = TemplateDir + "/dep_data_init.tpl";
	std::string paramDepInitTemplate = TemplateDir + "/dep_param_init.tpl";

	std::string hardwareThreadName    = "hwt_mehari";
	std::string hardwareThreadVersion = "v1_00_a";

	std::string mehariOutput      = OutputDir + "/linux/" + "mehari.c";
	std::string hardwareThreadDir = OutputDir + "/hw/" + hardwareThreadName + "_" + hardwareThreadVersion;
	std::string fpgaCalcOutput = hardwareThreadDir + "/hdl/vhdl/" + "calculation.vhd";
	std::string reconosOutput  = hardwareThreadDir + "/hdl/vhdl/" + hardwareThreadName + ".vhd";

	// use the template write to fill template
	TemplateWriter tWriter;

	// write extern global variables
	SimpleCCodeGenerator codeGen;
	std::stringstream globVarOutput;
	for (std::vector<SimpleCCodeGenerator::GlobalArrayVariable>::iterator gvIt = globalVariables.begin(); gvIt != globalVariables.end(); ++gvIt)
		codeGen.createExternArray(globVarOutput, *gvIt);
	tWriter.setValue("GLOBAL_VARIABLES", globVarOutput.str());

	// write number of used semaphores
	std::string semCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << (semNumberMax)))->str();
	tWriter.setValue("SEM_COUNT", semCountStr);

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

	// insert thread count
	unsigned int threadCount = 0;
	for (std::map<std::string, unsigned int>::iterator it = partitioningNumbers.begin(); it != partitioningNumbers.end(); ++it)
		threadCount += (it->second - 1);
	std::string threadCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << threadCount))->str();
	tWriter.setValue("THREAD_COUNT", threadCountStr);

	// count threads during function writing
	unsigned int threadNumber = 0;

	// create an instance of HardwareInformation
	boost::scoped_ptr<HardwareInformation> hw_info(new HardwareInformation());

	// write partitioning for each function
	unsigned int functionIndex = 0;
	unsigned int putParamStart = 0;
	unsigned int hwThreadCount = 0;
	for (std::map<std::string, Function*>::iterator funcIt = functions.begin(); funcIt != functions.end(); ++funcIt, functionIndex++) {
		std::string currentFunction = funcIt->first;
		std::string currentFunctionUppercase = boost::to_upper_copy(currentFunction);
		std::string functionIndexStr = static_cast<std::ostringstream*>( &(std::ostringstream() << functionIndex))->str();

		std::string functionTemplate = TemplateDir + "/" + currentFunction + "_func.tpl";
		std::string funcCallTemplate = TemplateDir + "/" + currentFunction + "_call.tpl";

		Function *func = funcIt->second;
		PartitioningGraph *pGraph = graphs[currentFunction];

		// insert partition count for this function
		std::string partitionCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << partitioningNumbers[currentFunction]))->str();
		tWriter.setValueInSubTemplate(partitionCountTemplate, "PARTITION_COUNT_ENTRIES", currentFunction + "_PARTITION_COUNT",
			"FUNCTION_NAME", currentFunctionUppercase);
		tWriter.setValueInSubTemplate(partitionCountTemplate, "PARTITION_COUNT_ENTRIES", currentFunction + "_PARTITION_COUNT",
			"PARTITION_COUNT", partitionCountStr);

		// enable the section for this function
		tWriter.enableSection(currentFunctionUppercase + "_IMPLEMENTATION");

		// add implementation to main calculation function
		std::string putParamStartStr = static_cast<std::ostringstream*>( &(std::ostringstream() << putParamStart))->str();
		tWriter.setValue(currentFunctionUppercase + "_PUT_PARAM_START",  putParamStartStr);

		// add initialization of parameter depdendencies
		for (unsigned int j=putParamStart; j<putParamStart+partitioningNumbers[currentFunction]-1; j++) {
			std::string depNum = static_cast<std::ostringstream*>( &(std::ostringstream() << j))->str();
			tWriter.setValueInSubTemplate(paramDepInitTemplate, "DEPENDENCY_INITIALIZATIONS",  depNum + "_DEP_PARAM_INIT",
				"DEPENDENCY_NUMBER", depNum);
			tWriter.setValueInSubTemplate(paramDepInitTemplate, "DEPENDENCY_INITIALIZATIONS",  depNum + "_DEP_PARAM_INIT",
				"FUNCTION_NAME", currentFunction);	
		}

		// collect the instructions for each partition
		std::vector<Instruction*> instructionsForPartition[partitioningNumbers[currentFunction]];
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

		// determine partition types
		//TODO We should get this information from the partitioning algorithms.
		std::vector<DeviceInformation::DeviceType> deviceTypes;
		BOOST_FOREACH(const std::string& device_type_name, partitioningDevices) {
			if (deviceTypes.size() >= partitioningNumbers[currentFunction])
				break;

			deviceTypes.push_back(hw_info->getDeviceInfo(device_type_name)->getType());
		}
		while (deviceTypes.size() < partitioningNumbers[currentFunction]) {
			deviceTypes.push_back(DeviceInformation::CPU_LINUX);
		}

		// generate C code for each partition of this function and write it into template
		for (unsigned int i=0; i<partitioningNumbers[currentFunction]; i++) {
			std::string partitionNumber = static_cast<std::ostringstream*>( &(std::ostringstream() << i))->str();
			std::string functionName = currentFunction + "_" + partitionNumber;

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
				std::string targetCoreStr = static_cast<std::ostringstream*>( &(std::ostringstream() << i))->str();
				tWriter.setValueInSubTemplate(threadImplTemplate, currentFunctionUppercase + "_THREADS", functionName + "_THREADS",
					"TARGET_CORE", targetCoreStr);
				// NOTE: this call sets a sub-template in a sub-template!
				tWriter.setValueInSubTemplate(funcCallTemplate, "FUNCTION_CALL", functionName + "_THREADS_CALL",
					"FUNCTION_NUMBER", partitionNumber, functionName + "_THREADS");
				
				// increment thread number for next thread creation
				threadNumber++;
			}
			// create new function
			tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", functionName + "_FUNCTIONS",
				"FUNCTION_NUMBER", partitionNumber);
			if (deviceTypes[i] == DeviceInformation::CPU_LINUX) {
				SimpleCCodeGenerator codeGen;
				std::string functionBody = codeGen.createCCode(*func, instructionsForPartition[i]);
				tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", functionName + "_FUNCTIONS",
					"FUNCTION_BODY", functionBody);
			} else {
				VHDLBackend *backend = new VHDLBackend("calculation");
				SimpleCCodeGenerator codeGen(backend);
				std::string vhdl_calculation = codeGen.createCCode(*func, instructionsForPartition[i]);

				// writeToFile creates the directory, if it doesn't exist
				TemplateWriter::writeToFile(fpgaCalcOutput, vhdl_calculation);
				saveOperator(reconosOutput, backend->getReconOSOperator(), hardwareThreadName);

				std::ostringstream body;
				body << "\tmbox_put(&mbox_start, 42);\n\n"
				     << prefixAllLines("\t", backend->getInterfaceCode()) << "\n"
				     << "\t(void)mbox_get(&mbox_stop);\n";

				tWriter.setValueInSubTemplate(functionTemplate, currentFunctionUppercase + "_FUNCTIONS", functionName + "_FUNCTIONS",
					"FUNCTION_BODY", body.str());


				GenerateHardwareThreadFileFromTemplate ghtfft = { TemplateDir, hardwareThreadDir,
						hardwareThreadName, hardwareThreadVersion };
				ghtfft.generate("hwt.mpd.tpl",    "data/" + hardwareThreadName + "_v2_1_0.mpd");
				ghtfft.generate("hwt.pao.tpl",    "data/" + hardwareThreadName + "_v2_1_0.pao");
				ghtfft.generate("hwt.tcl.tpl",    "data/" + hardwareThreadName + "_v2_1_0.tcl");
				ghtfft.generate("setup_zynq.tpl", "../setup_zynq");

				// increment hardware thread count to write it to template
				hwThreadCount++;
			}
		}

		// update parameter start index for the next function
		putParamStart += (partitioningNumbers[currentFunction]-1);
	}

	// after counting all hardware threads now we can insert the number into the template
	std::string hwThreadCountStr = static_cast<std::ostringstream*>( &(std::ostringstream() << hwThreadCount))->str();
	tWriter.setValue("THREAD_COUNT_HW", hwThreadCountStr);

	// expand and save template
	tWriter.expandTemplate(mehariTemplate);
	tWriter.writeToFile(mehariOutput);
}


// register pass so we can call it using opt
char Partitioning::ID = 0;
static RegisterPass<Partitioning>
Y("partitioning", "Create a partitioning for the calculation inside the function.");
