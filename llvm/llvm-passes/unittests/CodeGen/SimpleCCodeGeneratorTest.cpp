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
#include "mehari/CodeGen/UnittestHelpers.h"

#include <vector>
#include <string>


using namespace llvm;

namespace {

class SimpleCCodeGeneratorTest : public CodeGeneratorTest {

protected:
  CodeGeneratorBackend* createBackend() {
    return new CCodeBackend();
  }

}; // end class SimpleCCodeGeneratorTest

} // end anonymous namespace


TEST_F(SimpleCCodeGeneratorTest, ParameterAssignmentTest) {
  ParseC(
    "void test(int a, int b) {"
    "  a = 1;"
    "  b = a;"
    "}");
  std::string ExpectedResult = 
    "\ta = 1;\n"
    "\tb = a;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, ParameterCalculationTest) {
  ParseC(
    "void test(int a, int b) {"
    "  b = a + 2;"
    "}");
  std::string ExpectedResult = 
    "\tint t0;\n"
    "\tt0 = a + 2;\n"
    "\tb = t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, ArrayParameterCalculationTest) {
  ParseC(
    "void test(double* a, double* b) {"
    "  a[1] = 1;"
    "  b[0] = a[1] + 2;"
    "}");
  std::string ExpectedResult = 
    "\tdouble t0;\n"
    "\ta[1] = 1;\n"
    "\tt0 = a[1] + 2;\n"
    "\tb[0] = t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, GlobalArrayCalculationTest) {
  ParseC(
    "double a[5], b[5];"
    "void test() {"
    "  a[0] = 1.5;"
    "  a[1] = b[0];"
    "  b[2] = a[0] + 2;"
    "}");
  std::string ExpectedResult = 
    "\tdouble t0;\n"
    "\ta[0] = 1.5;\n"
    "\ta[1] = b[0];\n"
    "\tt0 = a[0] + 2;\n"
    "\tb[2] = t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, FunctionCallWithReturnTest) {
  ParseC(
    "int func(int);"
    "void test(int a) {"
    "  a = func(a);"
    "}");
  std::string ExpectedResult = 
    "\tint t0;\n"
    "\tt0 = func(a);\n"
    "\ta = t0;\n";
  CheckResult(ExpectedResult);
}

TEST_F(SimpleCCodeGeneratorTest, FunctionCallWithoutReturnTest) {
  ParseC(
    "void func2(int, int);"
    "void test(int a) {"
    "  func2(a, 2);"
    "}");
  std::string ExpectedResult = 
    "\tfunc2(a, 2);\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, ReturnValueTest) {
  ParseC(
    "double test(double a) {"
    "  return a+2;"
    "}");
  std::string ExpectedResult = 
    "\tdouble t0;\n"
    "\tt0 = a + 2;\n"
    "\treturn t0;\n";
  CheckResult(ExpectedResult);
}


TEST_F(SimpleCCodeGeneratorTest, SingleIfTest) {
  ParseC(
    "void test(int a, int b) {"
    "  if (a)"
    "    b = 2;"
    "}");
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
  ParseC(
    "void test(int a, int b, int c) {"
    "  a = 42;"
    "  b = 1;"
    "  c = (a > 0 ? a : b);"
    "}");
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
