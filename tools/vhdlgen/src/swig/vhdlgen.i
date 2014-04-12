%module vhdlgen

%{
#include "vhdlgen.h"

using boost::shared_ptr;
%}

%include "std_string.i"
%include "std_vector.i"
%include "std_set.i"
%include "std_map.i"
%include <boost_shared_ptr.i>

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
%shared_ptr(vhdl::Signal)
%shared_ptr(vhdl::Constant)
%shared_ptr(vhdl::Variable)
%shared_ptr(vhdl::LocalValueDeclaration)
%shared_ptr(vhdl::Component)
%shared_ptr(vhdl::Statement)
%shared_ptr(vhdl::Instance)

%shared_ptr(std::map< std::string,vhdl::UsedLibrary >)

namespace std {
   %template(topleveldeclarationvector) vector< boost::shared_ptr< vhdl::ToplevelDeclaration > >;
   %template(usedlibrarymap) map< std::string,vhdl::UsedLibrary >;
};

%import "pprint.i"

%extend vhdl::UsedLibrary {
	vhdl::UsedLibrary& use(const std::string& element) {
		*$self << element;
		return *$self;
	}
}

%ignore vhdl::Port::operator[];
%extend vhdl::Port {
	const Pin& __getitem__(const std::string& name) const {
		return (*$self)[name];
	}
}

// This doesn't work, so we have to use "#ifndef SWIG" in vhdlgen.h:
//%delobject vhdl::CompilationUnit::add;
//%rename ("$ignore", fullname=1) vhdl::CompilationUnit::add(vhdl::ToplevelDeclaration* decl);

%include "vhdlgen.h"
