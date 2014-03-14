#include "mehari/Transforms/AddAlwaysInlineAttributePass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Attributes.h"

#include <vector>
#include <algorithm>


AddAlwaysInlineAttributePass::AddAlwaysInlineAttributePass() : ModulePass(ID) {}
AddAlwaysInlineAttributePass::~AddAlwaysInlineAttributePass() {}


bool AddAlwaysInlineAttributePass::runOnModule(Module &M) {
 	
 	// for now just insert the target functions here, later use command line parameter for this
	std::vector<std::string> targetFunctions;
	targetFunctions.push_back("evalParameterCouplings");

	bool modified = false;

    for (Module::iterator funcIt = M.begin(); funcIt != M.end(); ++funcIt) {
    	if (std::find(targetFunctions.begin(), targetFunctions.end(), funcIt->getName()) != targetFunctions.end()) {
        	funcIt->addFnAttr(Attribute::AlwaysInline);
        	modified = true;
        }
    }
    return modified;
}

// register pass so we can call it using opt
char AddAlwaysInlineAttributePass::ID = 0;
static RegisterPass<AddAlwaysInlineAttributePass>
Y("add-attr-always-inline", "Add the AlwaysInline attribute to a set of functions.");
