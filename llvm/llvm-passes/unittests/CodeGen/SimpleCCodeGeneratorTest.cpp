#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
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


  void CheckResult(std::string &ExpectedResult, bool printResults = false) {
    // create worklist containing all instructions of the function
    std::vector<Instruction*> worklist;
    for (inst_iterator I = inst_begin(*F), E = inst_end(*F); I != E; ++I)
      worklist.push_back(&*I);

    // remove parameter initialization from the instruction vector
    unsigned int paramCount = 0;
    for (std::vector<Instruction*>::iterator instrIt = worklist.begin(); instrIt != worklist.end(); ++instrIt)
      if (isa<AllocaInst>(*instrIt))
        paramCount++;
    worklist.erase(worklist.begin(), worklist.begin()+2*paramCount);

    // run C code generator
    SimpleCCodeGenerator codeGen;
    std::string CodeGenResult = codeGen.createCCode(*F, worklist);

    if (printResults) {
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


TEST_F(SimpleCCodeGeneratorTest, ParameterAssignmentTest) {
  ParseAssembly(
    "define void @test(i32 %a, i32 %b) #0 {\n"
    "entry:\n"
    "  %a.addr = alloca i32, align 4\n"
    "  %b.addr = alloca i32, align 4\n"
    "  store i32 %a, i32* %a.addr, align 4\n"
    "  store i32 %b, i32* %b.addr, align 4\n"
    "  store i32 1, i32* %a.addr, align 4\n"
    "  %0 = load i32* %a.addr, align 4\n"
    "  store i32 %0, i32* %b.addr, align 4\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult = 
    "\ta = 1;\n"
    "\tb = a;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, ParameterCalculationTest) {
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


TEST_F(SimpleCCodeGeneratorTest, ArrayParameterCalculationTest) {
  ParseAssembly(
    "define void @test(double* %a, double* %b) #0 {\n"
    "entry:\n"
    "  %a.addr = alloca double*, align 8\n"
    "  %b.addr = alloca double*, align 8\n"
    "  store double* %a, double** %a.addr, align 8\n"
    "  store double* %b, double** %b.addr, align 8\n"
    "  %0 = load double** %a.addr, align 8\n"
    "  %arrayidx = getelementptr inbounds double* %0, i64 1\n"
    "  store double 1.000000e+00, double* %arrayidx, align 8\n"
    "  %1 = load double** %a.addr, align 8\n"
    "  %arrayidx1 = getelementptr inbounds double* %1, i64 1\n"
    "  %2 = load double* %arrayidx1, align 8\n"
    "  %add = fadd double %2, 2.000000e+00\n"
    "  %3 = load double** %b.addr, align 8\n"
    "  %arrayidx2 = getelementptr inbounds double* %3, i64 0\n"
    "  store double %add, double* %arrayidx2, align 8\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult = 
    "\tdouble t0;\n"
    "\ta[1] = 1;\n"
    "\tt0 = a[1] + 2;\n"
    "\tb[0] = t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, GlobalArrayCalculationTest) {
  ParseAssembly(
    "@a = common global [5 x double] zeroinitializer, align 16\n"
    "@b = common global [5 x double] zeroinitializer, align 16\n"
    "define void @test() #0 {\n"
    "entry:\n"
    "  store double 1.500000e+00, double* getelementptr inbounds ([5 x double]* @a, i32 0, i64 0), align 8\n"
    "  %0 = load double* getelementptr inbounds ([5 x double]* @b, i32 0, i64 0), align 8\n"
    "  store double %0, double* getelementptr inbounds ([5 x double]* @a, i32 0, i64 1), align 8\n"
    "  %1 = load double* getelementptr inbounds ([5 x double]* @a, i32 0, i64 0), align 8\n"
    "  %add = fadd double %1, 2.000000e+00\n"
    "  store double %add, double* getelementptr inbounds ([5 x double]* @b, i32 0, i64 2), align 8\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult = 
    "\tdouble t0;\n"
    "\ta[0] = 1.5;\n"
    "\ta[1] = b[0];\n"
    "\tt0 = a[0] + 2;\n"
    "\tb[2] = t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, FunctionCallTest) {
  ParseAssembly(
    "declare i32 @func(i32) #1\n"
    "define void @test(i32 %a) #0 {\n"
    "entry:\n"
    "  %a.addr = alloca i32, align 4\n"
    "  store i32 %a, i32* %a.addr, align 4\n"
    "  %0 = load i32* %a.addr, align 4\n"
    "  %call = call i32 @func(i32 %0)\n"
    "  store i32 %call, i32* %a.addr, align 4\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult = 
    "\tint t0;\n"
    "\tt0 = func(a);\n"
    "\ta = t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, SingleIfTest) {
  ParseAssembly(
    "define void @test(i32 %a, i32 %b) #0 {\n"
    "entry:\n"
    "  %a.addr = alloca i32, align 4\n"
    "  %b.addr = alloca i32, align 4\n"
    "  store i32 %a, i32* %a.addr, align 4\n"
    "  store i32 %b, i32* %b.addr, align 4\n"
    "  %0 = load i32* %a.addr, align 4\n"
    "  %tobool = icmp ne i32 %0, 0\n"
    "  br i1 %tobool, label %if.then, label %if.end\n"
    "if.then:                                          ; preds = %entry\n"
    "  store i32 2, i32* %b.addr, align 4\n"
    "  br label %if.end\n"
    "if.end:                                           ; preds = %if.then, %entry\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult =
    "\tint t0;\n"
    "\tt0 = (a != 0);\n"
    "\tif (t0) goto label1; else goto label0;\n"
    "label1:\n"
    "\tb = 2;\n"
    "\tgoto label2;\n"
    "label0:\n"
    "label2:\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, TernaryOperatorTest) {
  ParseAssembly(
    "define void @test(i32 %a, i32 %b, i32 %c) #0 {\n"
    "entry:\n"
    "  %a.addr = alloca i32, align 4\n"   "\n"
    "  %b.addr = alloca i32, align 4\n"
    "  %c.addr = alloca i32, align 4\n"
    "  store i32 %a, i32* %a.addr, align 4\n"
    "  store i32 %b, i32* %b.addr, align 4\n"
    "  store i32 %c, i32* %c.addr, align 4\n"
    "  store i32 42, i32* %a.addr, align 4\n"
    "  store i32 1, i32* %b.addr, align 4\n"
    "  %0 = load i32* %a.addr, align 4\n"
    "  %cmp = icmp sgt i32 %0, 0\n"
    "  br i1 %cmp, label %cond.true, label %cond.false\n"
    "cond.true:                                        ; preds = %entry\n"
    "  %1 = load i32* %a.addr, align 4\n"
    "  br label %cond.end\n"
    "cond.false:                                       ; preds = %entry\n"
    "  %2 = load i32* %b.addr, align 4\n"
    "  br label %cond.end\n"
    "cond.end:                                         ; preds = %cond.false, %cond.true\n"
    "  %cond = phi i32 [ %1, %cond.true ], [ %2, %cond.false ]\n"
    "  store i32 %cond, i32* %c.addr, align 4\n"
    "  ret void\n"
    "}\n");
  std::string ExpectedResult =
    "\tint t0, t1;\n"
    "\ta = 42;\n"
    "\tb = 1;\n"
    "\tt0 = (a > 0);\n"
    "\tif (t0) goto label1; else goto label0;\n"
    "label1:\n"
    "\tt1 = a;\n"
    "\tgoto label2;\n"
    "label0:\n"
    "\tt1 = b;\n"
    "\tgoto label3;\n"
    "label2:\n"
    "label3:\n"
    "\tc = t1;\n";
  CheckResult(ExpectedResult);
}
