#include "vhdlgen.h"

namespace vhdl {

UsedLibrary::UsedLibrary(std::string name) : name(name) { }

UsedLibrary& UsedLibrary::operator << (std::string element) {
	this->push_back(element);
	return *this;
}

const std::string& UsedLibrary::getName() {
	return this->name;
}

UsedLibrary& UsedLibraries::add(std::string name) {
	UsedLibrary lib(name);
	UsedLibrary& lib_in_map = (*this)[name];
	lib_in_map = lib;
	return lib_in_map;
}

}
