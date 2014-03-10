#ifndef __HEADER_VHDLGEN__
#define __HEADER_VHDLGEN__

#include <string>
#include <vector>
#include <map>

#include "pprint_builder.h"

namespace vhdl {

class UsedLibrary : public std::vector<std::string>, public pprint::PrettyPrintable {
	std::string name;
public:
	UsedLibrary(std::string name = "");

	UsedLibrary& operator << (std::string element);

	const std::string& getName();

	virtual const pprint::PrettyPrinted_p prettyPrint() const;
};

class UsedLibraries : public std::map<std::string, UsedLibrary>, public pprint::PrettyPrintable {
public:
	UsedLibrary& add(std::string name);

	virtual const pprint::PrettyPrinted_p prettyPrint() const;
};

class VHDLFile {
public:
	UsedLibraries libraries;
};

}

#endif // not defined __HEADER_VHDLGEN__
