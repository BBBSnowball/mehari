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
#include "mehari/CodeGen/GenerateVHDL.h"

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdio>


using namespace llvm;

namespace {

class SimpleVHDLGeneratorTest : public testing::Test {

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


  void ParseC(const char *Code) {
    char prefix[] = "ccode.XXXXXXXX";
    mktemp(prefix);

    std::string codefile = std::string(prefix) + ".c";
    writeFile(codefile, Code);

    std::string command = "./clang -S -emit-llvm '" + codefile + "'";
    int result = system(command.c_str());
    EXPECT_EQ(result, 0);
    if (result != 0)
      throw std::runtime_error("clang returned a non-zero status.");

    std::string assemblyfile = std::string(prefix) + ".ll";
    std::string assembly = readFile(assemblyfile);

    remove(codefile.c_str());
    remove(assemblyfile.c_str());

    ParseAssembly(assembly.c_str());
  }


  void CheckResult(const std::string &ExpectedResult, bool printResults = false) {
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
    SimpleCCodeGenerator codeGen(new VHDLBackend());
    std::string CodeGenResult = codeGen.createCCode(*F, worklist);

    if (printResults) {
      errs() << "### Expected Result:\n" << ExpectedResult << "\n";
      errs() << "### Generated Result:\n" << CodeGenResult << "\n";
    }

    if (ExpectedResult != CodeGenResult) {
      writeFile("expected", ExpectedResult);
      writeFile("actual",   CodeGenResult);
    }

    // compare results
    EXPECT_EQ(ExpectedResult, CodeGenResult);
  }

  std::string fromFile(const std::string& filename) {
    std::string full_filename = "CodeGen_data/" + filename;
    return readFile(full_filename);
  }

  std::string readFile(const std::string& filename) {
    std::ifstream file;
    file.open(filename.c_str(), std::ios::in | std::ios::binary);

    if (file) {
      std::string contents;
      file.seekg(0, std::ios::end);
      contents.resize(file.tellg());
      file.seekg(0, std::ios::beg);
      file.read(&contents[0], contents.size());
      file.close();
      return contents;
    }

    throw(errno);
  }

  void writeFile(const std::string& filename, const std::string& contents) {
    std::ofstream file;
    file.open(filename.c_str(), std::ios::out);
    file << contents;
    file.close();
  }

  void CheckResultFromFile(const std::string& filename, bool printResults = false) {
    CheckResult(fromFile(filename), printResults);
  }

private:
  Module *M;
  Function *F;

}; // end class SimpleVHDLGeneratorTest

} // end anonymous namespace


TEST_F(SimpleVHDLGeneratorTest, ParameterAssignmentTest) {
  ParseC(
    "void test(int a, int b) {"
    "  a = 1;"
    "  b = a;"
    "}");
  CheckResultFromFile("ParameterAssignmentTest.vhdl");
}


TEST_F(SimpleVHDLGeneratorTest, ParameterCalculationTest) {
  ParseC(
    "void test(int a, int b) {"
    "  b = a + 2;"
    "}");
  CheckResultFromFile("ParameterCalculationTest.vhdl");
}


TEST_F(SimpleVHDLGeneratorTest, ArrayParameterCalculationTest) {
  ParseC(
    "void test(double* a, double* b) {"
    "  a[1] = 1;"
    "  b[0] = a[1] + 2;"
    "}");
  CheckResultFromFile("ArrayParameterCalculationTest.vhdl");
}


TEST_F(SimpleVHDLGeneratorTest, GlobalArrayCalculationTest) {
  ParseC(
    "double a[5], b[5];"
    "void test() {"
    "  a[0] = 1.5;"
    "  a[1] = b[0];"
    "  b[2] = a[0] + 2;"
    "}");
  CheckResultFromFile("GlobalArrayCalculationTest.vhdl");
}


TEST_F(SimpleVHDLGeneratorTest, FunctionCallWithReturnTest) {
  ParseC(
    "int func(int);"
    "void test(int a) {"
    "  a = func(a);"
    "}");
  CheckResultFromFile("FunctionCallWithReturnTest.vhdl");
}


TEST_F(SimpleVHDLGeneratorTest, FunctionCallWithoutReturnTest) {
  ParseC(
    "void func(int, int);"
    "void test(int a) {"
    "  func(a, 2);"
    "}");
  CheckResultFromFile("FunctionCallWithoutReturnTest.vhdl");
}


/*TEST_F(SimpleVHDLGeneratorTest, SingleIfTest) {
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
    "\tif (t0) goto label0; else goto label1;\n"
    "label0:\n"
    "\tb = 2;\n"
    "\tgoto label2;\n"
    "label1:\n"
    "label2:\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleVHDLGeneratorTest, TernaryOperatorTest) {
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
    "\tif (t0) goto label0; else goto label1;\n"
    "label0:\n"
    "\tt1 = a;\n"
    "\tgoto label2;\n"
    "label1:\n"
    "\tt1 = b;\n"
    "\tgoto label3;\n"
    "label2:\n"
    "label3:\n"
    "\tc = t1;\n";
  CheckResult(ExpectedResult);
}*/
