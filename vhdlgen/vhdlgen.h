#ifndef __HEADER_VHDLGEN__
#define __HEADER_VHDLGEN__

#include <string>
#include <vector>
#include <map>

namespace vhdl {

class UsedLibrary : public std::vector<std::string> {
	std::string name;
public:
	UsedLibrary(std::string name = "");

	UsedLibrary& operator << (std::string element);

	const std::string& getName();
};

class UsedLibraries : public std::map<std::string, UsedLibrary> {
public:
	UsedLibrary& add(std::string name);
};

class VHDLFile {
public:
	UsedLibraries libraries;
};

}

#endif // not defined __HEADER_VHDLGEN__
