#ifndef ADD_ALWAYS_INLINE_ATTRIBUTE_PASS_H
#define ADD_ALWAYS_INLINE_ATTRIBUTE_PASS_H

#include "llvm/Pass.h"

using namespace llvm;

class AddAlwaysInlineAttributePass : public ModulePass {

public:
  static char ID;

  AddAlwaysInlineAttributePass();
  ~AddAlwaysInlineAttributePass();

  virtual bool runOnModule(Module &M);

private:
  std::vector<std::string> targetFunctions;
  void parseTargetFunctions();
};

#endif /*ADD_ALWAYS_INLINE_ATTRIBUTE_PASS_H*/
