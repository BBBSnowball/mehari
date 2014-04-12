#ifndef HARDWARE_INFORMATION_H_
#define HARDWARE_INFORMATION_H_

#include <map>
#include <string>


class InstructionInformation {
public:
	InstructionInformation(std::string name, unsigned int cycles);
	~InstructionInformation();

	unsigned int getCycleCount(void);

private:
	std::string opcodeName;
	unsigned int opcode;
	unsigned int cycleCount;
};



class DeviceInformation {
public:
	DeviceInformation(std::string deviceName);
	~DeviceInformation();

	void addInstructionInfo(std::string opcodeName, unsigned int cycles);
	InstructionInformation *getInstructionInfo(const char* opcodeName);

private:
	std::string name;
	std::map<std::string, InstructionInformation*> *instrInfoMap;
};



class HardwareInformation {
public:
	HardwareInformation();
	~HardwareInformation();
	
	DeviceInformation *getDeviceInfo(const char* deviceName);

private:
	std::map<std::string, DeviceInformation*> *devices;
};

#endif /*HARDWARE_INFORMATION_H_*/
