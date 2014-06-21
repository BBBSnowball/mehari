#include "mehari/HardwareInformation.h"
#include "mehari/utils/ContainerUtils.h"

#include "llvm/IR/Function.h"

#include <cstddef>

#include <boost/assign.hpp>


namespace {
	// NOTE: The opcodes for the instructions used here are specific for the LLVM intermediate representation.
	// 		 They are defined in the LLVM source tree in include/llvm/IR/Instruction.def
	// 		 Here we store a mapping opcodeName->opcode,
	//		 because we can not get that mapping from the enums in Instruction.def
	std::map<std::string, unsigned int> opcodesMap = boost::assign::map_list_of 
		("ret",     1)
		("br",		2)
		("fadd",    9) 
		("fsub",   11) 
		("fmul",   13) 
		("fdiv",   16)
		("or",     24)
		("alloca", 26)
		("load",   27)
		("store",  28)
		("getelementptr", 29)
		("zext",   34)
		("icmp",   46)
		("fcmp",   47)
		("phi",	   48)
		("call",   49);
}



// HardwareInformation
// -------------------
HardwareInformation::HardwareInformation() {
	devices = new std::map<std::string, DeviceInformation*>();

	// the FPGA runs @  100MHz instead of 800MHz, so we need to corrent 
	// the timinigs for a comparison with the ARM processor
	unsigned int fpgaClockMultiplier = 8;


	// add timinigs for the ARM Cortex-A9 @ 800MHz
	DeviceInformation *cortexA9 = new DeviceInformation("Cortex-A9", DeviceInformation::CPU_LINUX);
	cortexA9->addInstructionInfo("ret",    0);
	cortexA9->addInstructionInfo("br",     0);
	cortexA9->addInstructionInfo("fadd",   4);
	cortexA9->addInstructionInfo("fsub",   4);
	cortexA9->addInstructionInfo("fmul",   6);
	cortexA9->addInstructionInfo("fdiv",  25);
	cortexA9->addInstructionInfo("or",     2);
	cortexA9->addInstructionInfo("alloca", 0);
	cortexA9->addInstructionInfo("load",   4);
	cortexA9->addInstructionInfo("store",  6);
	cortexA9->addInstructionInfo("getelementptr", 3);
	cortexA9->addInstructionInfo("zext",   1);
	cortexA9->addInstructionInfo("icmp",   3);
	cortexA9->addInstructionInfo("fcmp",   4);
	cortexA9->addInstructionInfo("select", 4);
	cortexA9->addInstructionInfo("phi",    0);
	cortexA9->addInstructionInfo("call",  50); // NOTE: approximation

	cortexA9->addCommunicationInfo("Cortex-A9", DataDependency,  455);
	cortexA9->addCommunicationInfo("Cortex-A9", OrderDependency, 220);
	cortexA9->addCommunicationInfo("xc7z020-1", DataDependency,  fpgaClockMultiplier * 275);
	cortexA9->addCommunicationInfo("xc7z020-1", OrderDependency, fpgaClockMultiplier * 115);

	devices->insert(std::pair<std::string, DeviceInformation*>(cortexA9->getName(), cortexA9));

	// add timinigs for the FPGA
	DeviceInformation *fpga = new DeviceInformation("xc7z020-1", DeviceInformation::FPGA_RECONOS);
	fpga->addInstructionInfo("ret",           fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("br",            fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("fadd",          fpgaClockMultiplier * 13);
	fpga->addInstructionInfo("fsub",          fpgaClockMultiplier * 13);
	fpga->addInstructionInfo("fmul",          fpgaClockMultiplier * 10);
	fpga->addInstructionInfo("fdiv",          fpgaClockMultiplier * 58);
	fpga->addInstructionInfo("or",            fpgaClockMultiplier * 1);
	fpga->addInstructionInfo("alloca",        fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("load",          fpgaClockMultiplier * 40);
	fpga->addInstructionInfo("store",         fpgaClockMultiplier * 40);
	fpga->addInstructionInfo("getelementptr", fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("zext",          fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("icmp",          fpgaClockMultiplier * 1);
	fpga->addInstructionInfo("fcmp",          fpgaClockMultiplier * 3);
	fpga->addInstructionInfo("select",        fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("phi",           fpgaClockMultiplier * 0);
	fpga->addInstructionInfo("call",          fpgaClockMultiplier * (1<<20)); 
	fpga->addInstructionInfo("call#sin",      fpgaClockMultiplier * (7+54+8));
	fpga->addInstructionInfo("call#cos",      fpgaClockMultiplier * (7+54+8));

	fpga->addCommunicationInfo("Cortex-A9", DataDependency,  fpgaClockMultiplier * 315);
	fpga->addCommunicationInfo("Cortex-A9", OrderDependency, fpgaClockMultiplier * 240);
	fpga->addCommunicationInfo("xc7z020-1", DataDependency,  fpgaClockMultiplier * 1);
	fpga->addCommunicationInfo("xc7z020-1", OrderDependency, fpgaClockMultiplier * 1);

	devices->insert(std::pair<std::string, DeviceInformation*>(fpga->getName(), fpga));


	// calculate the device independent communication costs 
	// -> average value of all devices for the communication cost types
	std::vector<std::string> targets;
	for (std::map<std::string, DeviceInformation*>::iterator it = devices->begin(); it != devices->end(); ++it)
		targets.push_back(it->first);

	unsigned int dataDepCostSum = 0, orderDepCostSum = 0, count = 0;
	for (std::map<std::string, DeviceInformation*>::iterator it = devices->begin(); it != devices->end(); ++it) {
		for (std::vector<std::string>::iterator targetIt = targets.begin(); targetIt != targets.end(); ++targetIt) {
			dataDepCostSum += it->second->getCommunicationInfo(*targetIt)->getCommunicationCost(DataDependency);
			orderDepCostSum += it->second->getCommunicationInfo(*targetIt)->getCommunicationCost(OrderDependency);
			count++;
		}
	}

	deviceIndependentComCosts[DataDependency] = dataDepCostSum/count;
	deviceIndependentComCosts[OrderDependency] = orderDepCostSum/count;
}


HardwareInformation::~HardwareInformation() {
	for (std::map<std::string, DeviceInformation*>::iterator it = devices->begin(); it != devices->end(); ++it)
  		delete it->second;
	delete devices;
}


DeviceInformation *HardwareInformation::getDeviceInfo(std::string deviceName) {
	return (*devices)[deviceName];
}


unsigned int HardwareInformation::getDeviceIndependentCommunicationCost(CommunicationType type) {
	return deviceIndependentComCosts[type];
}



// DeviceInformation
// -----------------
DeviceInformation::DeviceInformation(std::string deviceName, DeviceType deviceType) {
	name = deviceName;
	type = deviceType;
	instrInfoMap = new std::map<std::string, InstructionInformation*>();
	comInfoMap = new std::map<std::string, CommunicationInformation*>();
}


DeviceInformation::~DeviceInformation() {
	// free instruction information mapping
	for (std::map<std::string, InstructionInformation*>::iterator it = instrInfoMap->begin(); it != instrInfoMap->end(); ++it)
  		delete it->second;
	delete instrInfoMap;
	// free communication information mapping
	for (std::map<std::string, CommunicationInformation*>::iterator it = comInfoMap->begin(); it != comInfoMap->end(); ++it)
  		delete it->second;
  	delete comInfoMap;
}


std::string DeviceInformation::getName(void) {
	return name;
}


DeviceInformation::DeviceType DeviceInformation::getType(void) {
	return type;
}


void DeviceInformation::addInstructionInfo(std::string opcodeName, unsigned int cycles) {
	instrInfoMap->insert(std::pair<std::string, InstructionInformation*>(
		opcodeName, new InstructionInformation(opcodeName, cycles)));
}


InstructionInformation *DeviceInformation::getInstructionInfo(llvm::Instruction *instr) {
	return getInstructionInfo(getOpcodeName(instr));
}


std::string DeviceInformation::getOpcodeName(llvm::Instruction *instr) {
	if (llvm::CallInst *cInstr = llvm::dyn_cast<llvm::CallInst>(instr))
		return "call#" + cInstr->getCalledFunction()->getName().str();
	else
		return instr->getOpcodeName();
}


InstructionInformation *DeviceInformation::getInstructionInfo(std::string opcodeName) {
	size_t pos;
	if (InstructionInformation *ii = getValueOrDefault(*instrInfoMap, opcodeName))
		return ii;
	else if ((pos = opcodeName.find('#')) != std::string::npos) {
		std::string opcodePrefix = opcodeName.substr(0, pos);
		assert(contains(*instrInfoMap, opcodePrefix));
		return (*instrInfoMap)[opcodePrefix];
	} 
	else {
		assert(false);
		return NULL;
	}
}


void DeviceInformation::addCommunicationInfo(std::string target, CommunicationType type, unsigned int cost) {
	if (comInfoMap->find(target) == comInfoMap->end())
		comInfoMap->insert(std::pair<std::string, CommunicationInformation*>(
			target, new CommunicationInformation(name, target, type, cost)));
	else
		comInfoMap->operator[](target)->addCommunicationCost(type, cost);
}


CommunicationInformation *DeviceInformation::getCommunicationInfo(std::string target) {
	return (*comInfoMap)[target];
}



// InstructionInformation
// ----------------------
InstructionInformation::InstructionInformation(std::string name, unsigned int cycles) {
	opcodeName = name;
	opcode = opcodesMap[name];
	cycleCount = cycles;
}


InstructionInformation::~InstructionInformation() {}


unsigned int InstructionInformation::getCycleCount(void) {
	return cycleCount;
}



// CommunicationInformation
// ------------------------

CommunicationInformation::CommunicationInformation(std::string comSource, std::string comTarget, 
	CommunicationType type, unsigned int cost) {
	source = comSource;
	target = comTarget;
	costs[type] = cost;
}


CommunicationInformation::~CommunicationInformation() {}


void CommunicationInformation::addCommunicationCost(CommunicationType type, unsigned int cost) {
	costs[type] = cost;
}


unsigned int CommunicationInformation::getCommunicationCost(CommunicationType type) {
	return costs[type];
}
