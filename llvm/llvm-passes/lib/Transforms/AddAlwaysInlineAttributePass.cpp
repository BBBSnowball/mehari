#include "mehari/Transforms/AddAlwaysInlineAttributePass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Support/CommandLine.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <vector>
#include <algorithm>


static cl::opt<std::string> TargetFunctions("inline-functions", 
            cl::desc("Specify the functions the always inline attribute will be added to (seperated by whitespace)"), 
            cl::value_desc("target-functions"));


AddAlwaysInlineAttributePass::AddAlwaysInlineAttributePass() : ModulePass(ID) {
  parseTargetFunctions();
}

AddAlwaysInlineAttributePass::~AddAlwaysInlineAttributePass() {}


bool AddAlwaysInlineAttributePass::runOnModule(Module &M) {
	bool modified = false;
    for (Module::iterator funcIt = M.begin(); funcIt != M.end(); ++funcIt) {
    	if (std::find(targetFunctions.begin(), targetFunctions.end(), funcIt->getName()) != targetFunctions.end()) {
        	funcIt->addFnAttr(Attribute::AlwaysInline);
        	modified = true;
        }
    }
    return modified;
}


void AddAlwaysInlineAttributePass::parseTargetFunctions() {
  boost::algorithm::split(targetFunctions, TargetFunctions, boost::algorithm::is_any_of(" "));
}


// register pass so we can call it using opt
char AddAlwaysInlineAttributePass::ID = 0;
static RegisterPass<AddAlwaysInlineAttributePass>
Y("add-attr-always-inline", "Add the AlwaysInline attribute to a set of functions.");
