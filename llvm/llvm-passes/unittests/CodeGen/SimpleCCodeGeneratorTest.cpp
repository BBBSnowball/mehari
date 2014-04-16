#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"

#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include <vector>
#include <string>


using namespace llvm;

namespace {

class SimpleCCodeGeneratorTest : public testing::Test {

protected:
  
  void ParseAssembly(const char *Assembly) {
    M = new Module("Module", getGlobalContext());

    SMDiagnostic Error;
    bool Parsed = ParseAssemblyString(Assembly, M, Error, M->getContext()) == M;

    std::string errMsg;
    raw_string_ostream os(errMsg);
    Error.print("", os);

    if (!Parsed) {
      // A failure here means that the test itself is buggy.
      report_fatal_error(os.str().c_str());
    }

    F = M->getFunction("test");
    if (F == NULL)
      report_fatal_error("Test must have a function named @test");
  }


  void CheckResult(std::string &ExpectedResult) {
    // create worklist containing all instructions of the function
    std::vector<Instruction*> worklist;
    for (inst_iterator I = inst_begin(*F), E = inst_end(*F); I != E; ++I)
      worklist.push_back(&*I);

    // run C code generator
    SimpleCCodeGenerator codeGen;
    std::string CodeGenResult = codeGen.createCCode(worklist);

    // DEBUG output
    if (false) {
      errs() << "### Expected Result:\n" << ExpectedResult << "\n";
      errs() << "### Generated Result:\n" << CodeGenResult << "\n";
    }

    // compare results
    EXPECT_EQ(ExpectedResult, CodeGenResult);
  }

private:
  Module *M;
  Function *F;

}; // end class SimpleCCodeGeneratorTest

} // end anonymous namespace


TEST_F(SimpleCCodeGeneratorTest, ParameterCalculation) {
  ParseAssembly(
    "define void @test(i32 %a, i32 %b) #0 {\n"
    "entry:\n"
    "  %a.addr = alloca i32, align 4\n"
    "  %b.addr = alloca i32, align 4\n"
    "  store i32 %a, i32* %a.addr, align 4\n"
    "  store i32 %b, i32* %b.addr, align 4\n"
    "  %0 = load i32* %a.addr, align 4\n"
    "  %add = add nsw i32 %0, 2\n"
    "  store i32 %add, i32* %b.addr, align 4\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult = 
    "\tint t0;\n"
    "\tt0 = a + 2;\n"
    "\tb = t0;\n";
  CheckResult(ExpectedResult);
}
