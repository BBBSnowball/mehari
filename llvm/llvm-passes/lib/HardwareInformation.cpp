#include "mehari/HardwareInformation.h"

#include "boost/assign.hpp"


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

	// add timinigs for the Cortex-A9
	DeviceInformation *cortexA9 = new DeviceInformation("Cortex-A9");
	cortexA9->addInstructionInfo("ret",    0);
	cortexA9->addInstructionInfo("br",     0);
	cortexA9->addInstructionInfo("fadd",   4);
	cortexA9->addInstructionInfo("fsub",   4);
	cortexA9->addInstructionInfo("fmul",   6);
	cortexA9->addInstructionInfo("fdiv",  25);
	cortexA9->addInstructionInfo("or",     0);
	cortexA9->addInstructionInfo("alloca", 0);
	cortexA9->addInstructionInfo("load",   3);
	cortexA9->addInstructionInfo("store",  6);
	cortexA9->addInstructionInfo("getelementptr", 1);
	cortexA9->addInstructionInfo("zext",   1);
	cortexA9->addInstructionInfo("icmp",   3);
	cortexA9->addInstructionInfo("fcmp",   4);
	cortexA9->addInstructionInfo("phi",    0);
	cortexA9->addInstructionInfo("call",  25); // TODO: how to get cycle timings for call instruction?

	cortexA9->addCommunicationInfo("Cortex-A9", DataDependency, 30);
	cortexA9->addCommunicationInfo("Cortex-A9", OrderDependency, 5);

	// TODO: add communication costs ARM->FPGA

	std::string deviceName = "Cortex-A9";
	devices->insert(std::pair<std::string, DeviceInformation*>(deviceName, cortexA9));


	// TODO: add timinigs for the FPGA


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
DeviceInformation::DeviceInformation(std::string deviceName) {
	name = deviceName;
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


void DeviceInformation::addInstructionInfo(std::string opcodeName, unsigned int cycles) {
	instrInfoMap->insert(std::pair<std::string, InstructionInformation*>(
		opcodeName, new InstructionInformation(opcodeName, cycles)));
}


InstructionInformation *DeviceInformation::getInstructionInfo(std::string opcodeName) {
	return (*instrInfoMap)[opcodeName];
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
