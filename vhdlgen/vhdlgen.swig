%module vhdlgen

%{
#include "pprint.h"
#include "vhdlgen.h"

using boost::shared_ptr;
%}

%include "std_string.i"
%include "std_vector.i"
%include "std_set.i"
%include "std_map.i"
%include <boost_shared_ptr.i>

%shared_ptr(pprint::PrettyPrintable)
%shared_ptr(pprint::PrettyPrinted)

%shared_ptr(pprint::Empty)
%shared_ptr(pprint::Text)
%shared_ptr(pprint::Keyword)
%shared_ptr(pprint::PrettyPrintedWithChildren)
%shared_ptr(pprint::VCat)
%shared_ptr(pprint::HCat)
%shared_ptr(pprint::VCatOverlapping)
%shared_ptr(pprint::Indent)

%shared_ptr(vhdl::PrettyPrintableV)
%shared_ptr(vhdl::PrettyPrintable)
%shared_ptr(vhdl::ToplevelDeclaration)
%shared_ptr(vhdl::LocalDeclaration)
%shared_ptr(vhdl::UsedLibrary)
%shared_ptr(vhdl::UsedLibraries)
%shared_ptr(vhdl::Direction)
%shared_ptr(vhdl::Type)
%shared_ptr(vhdl::Value)
%shared_ptr(vhdl::ValueDeclaration)
%shared_ptr(vhdl::ValueDeclarationWithOptionalDefault)
%shared_ptr(vhdl::Pin)
%shared_ptr(vhdl::Port)
%shared_ptr(vhdl::Comment)
%shared_ptr(vhdl::Entity)
%shared_ptr(vhdl::Architecture)

%shared_ptr(std::vector< pprint::PrettyPrinted_p >)
%shared_ptr(std::set< std::string >)
%shared_ptr(std::map< std::string,vhdl::UsedLibrary >)

namespace std {
   %template(pprintedvector) vector<pprint::PrettyPrinted_p>;
   %template(topleveldeclarationvector) vector< boost::shared_ptr< vhdl::ToplevelDeclaration > >;
   %template(usedlibrarymap) map< std::string,vhdl::UsedLibrary >;
   %template(stringset) set<std::string>;
};

%rename(print_line) print;

%ignore vhdl::Port::operator[];
%extend vhdl::Port {
	const Pin& __getitem__(const std::string& name) const {
		return (*$self)[name];
	}
}

%include "pprint.h"
%include "vhdlgen.h"
