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

	// TODO: check cycle timings for the Cortex-A9 instructions in datasheet
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

	devices->insert(std::pair<std::string, DeviceInformation*>("Cortex-A9", cortexA9));
}


HardwareInformation::~HardwareInformation() {
	for (std::map<std::string, DeviceInformation*>::iterator it = devices->begin(); it != devices->end(); ++it)
  		delete it->second;
	delete devices;
}


DeviceInformation *HardwareInformation::getDeviceInfo(const char* deviceName) {
	return (*devices)[deviceName];
}



// DeviceInformation
// -------------------
DeviceInformation::DeviceInformation(std::string deviceName) {
	name = deviceName;
	instrInfoMap = new std::map<std::string, InstructionInformation*>();
}


DeviceInformation::~DeviceInformation() {
	for (std::map<std::string, InstructionInformation*>::iterator it = instrInfoMap->begin(); it != instrInfoMap->end(); ++it)
  		delete it->second;
	delete instrInfoMap;
}


void DeviceInformation::addInstructionInfo(std::string opcodeName, unsigned int cycles) {
	instrInfoMap->insert(std::pair<std::string, InstructionInformation*>(
		opcodeName, new InstructionInformation(opcodeName, cycles)));
}


InstructionInformation *DeviceInformation::getInstructionInfo(const char* opcodeName) {
	return (*instrInfoMap)[opcodeName];
}



// InstructionInformation
// -------------------
InstructionInformation::InstructionInformation(std::string name, unsigned int cycles) {
	opcodeName = name;
	opcode = opcodesMap[name];
	cycleCount = cycles;
}


InstructionInformation::~InstructionInformation() {}


unsigned int InstructionInformation::getCycleCount(void) {
	return cycleCount;
}
