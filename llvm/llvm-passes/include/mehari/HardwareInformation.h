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


enum CommunicationType {
	OrderDependency,
	DataDependency
};


class CommunicationInformation {
public:
	CommunicationInformation(std::string comSource, std::string comTarget, CommunicationType type, unsigned int cost);
	~CommunicationInformation();

	void addCommunicationCost(CommunicationType type, unsigned int cost);
	unsigned int getCommunicationCost(CommunicationType type);

private:
	std::string source;
	std::string target;
	std::map<CommunicationType, unsigned int> costs;
};


class DeviceInformation {
public:
	DeviceInformation(std::string deviceName);
	~DeviceInformation();

	void addInstructionInfo(std::string opcodeName, unsigned int cycles);
	InstructionInformation *getInstructionInfo(std::string opcodeName);

	void addCommunicationInfo(std::string target, CommunicationType type, unsigned int cost);
	CommunicationInformation *getCommunicationInfo(std::string target);

private:
	std::string name;
	std::map<std::string, InstructionInformation*> *instrInfoMap;
	std::map<std::string, CommunicationInformation*> *comInfoMap;
};


class HardwareInformation {
public:
	HardwareInformation();
	~HardwareInformation();
	
	DeviceInformation *getDeviceInfo(std::string deviceName);

private:
	std::map<std::string, DeviceInformation*> *devices;
};

#endif /*HARDWARE_INFORMATION_H_*/
