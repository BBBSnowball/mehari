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

const pprint::PrettyPrinted_p UsedLibrary::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;
	builder.append()
		.columns()
			.add("library")
			.add(" ")
			.add(name)
			.add(";")
			.up();

	for (const_iterator iter = this->begin(); iter != this->end(); ++iter)
		builder.columns()
			.add("use")
			.add(" ")
			.add(name + "." + *iter)
			.add(";")
			.up();

	return builder.build();
}

const pprint::PrettyPrinted_p UsedLibraries::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;

	for (const_iterator iter = this->begin(); iter != this->end(); ++iter)
		builder.append()
			.seperateBy("")
			.add(iter->second);

	return builder.build();
}

}
