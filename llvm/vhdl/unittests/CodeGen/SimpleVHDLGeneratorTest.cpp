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
#include "mehari/CodeGen/GenerateVHDL.h"
#include "mehari/CodeGen/ReconOSOperator.h"

#include "Operator.hpp"
#include "Signal.hpp"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <boost/foreach.hpp>


using namespace llvm;

namespace {

#include "TestOperator.incl.cpp"

class SimpleVHDLGeneratorTest : public CodeGeneratorTest {

protected:

  class TestOperator* makeTestOperator(const std::string& name) {
    assert(!testOp);
    testOp = new TestOperator(name, getGeneratedOperator());
    return testOp;
  }

  void saveTestOperator(const std::string& filename) {
    saveOperator(filename, testOp);
  }

  void saveOperator(const std::string& filename, ::Operator* op) {
    std::ofstream file;
    file.open(filename.c_str(), std::ios::out);
    op->outputVHDL(file, op->getName());
    file.close();
  }

  class TestOperator* makeTestOperator() { return makeTestOperator(getCurrentTestName()); }
  void saveTestOperator() { saveTestOperator(getCurrentTestName() + ".vhdl"); }

  MyOperator* getGeneratedOperator() {
    assert(((VHDLBackend*)backend)->getOperator());
    return ((VHDLBackend*)backend)->getOperator();
  }

public:
  SimpleVHDLGeneratorTest() : testOp(NULL) { }

private:
  TestOperator *testOp;

  CodeGeneratorBackend* createBackend() {
    backend = new VHDLBackend();
    return backend;
  }

}; // end class SimpleVHDLGeneratorTest

} // end anonymous namespace


TEST_F(SimpleVHDLGeneratorTest, ParameterAssignmentTest) {
  ParseC(
    "void test(int a, int b) {"
    "  a = 1;"
    "  b = a;"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitForAndCheckUnsignedIntegerResult("a_out", "1");
  test->waitForAndCheckUnsignedIntegerResult("b_out", "1", 0);
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, ParameterCalculationTest) {
  ParseC(
    "void test(int a, int b) {"
    "  b = a + 2;"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 3);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("b_out", "5");
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, ArrayParameterCalculationTest) {
  ParseC(
    "void test(double* a, double* b) {"
    "  a[1] = 1;"
    "  b[0] = a[1] + 2;"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitForAndCheckFloatResult("b_0_out", "3.0");
  test->waitForAndCheckFloatResult("a_1_out", "1.0", 0);
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, GlobalArrayCalculationTest) {
  ParseC(
    "double a[5], b[5];"
    "void test() {"
    "  a[0] = 1.5;"
    "  a[1] = b[0];"
    "  b[2] = a[0] + 2;"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitUntilReady();
  test->startDataInput();
  test->setFloatInput("b_0_in", 1.7);
  test->endDataInput();
  test->waitForAndCheckFloatResult("a_0_out", "1.5");
  //TODO: test->waitForAndCheckFloatResult("a_1_out", "1.7");
  test->waitForAndCheckFloatResult("b_2_out", "3.5", 0);
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, FunctionCallWithReturnTest) {
  ParseC(
    "int func(int);"
    "void test(int a) {"
    "  a = func(a);"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 2);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("a_out", "2*3+1");
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, FunctionCallWithoutReturnTest) {
  ParseC(
    "void func2(int, int);"
    "void test(int a) {"
    "  func2(a, 2);"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 2);
  test->endDataInput();
  //TODO We should check the result, but we don't even know when it is done.
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, ReturnValueTest) {
  ParseC(
    "double test(double a) {"
    "  return a+2;"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitUntilReady();
  test->startDataInput();
  test->setFloatInput("a_in", 7);
  test->endDataInput();
  test->waitForAndCheckFloatResult("return", "9.0");
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, UseParameterMoreThanOnce) {
  ParseC(
    "void func(int, int);"
    "void test(int a) {"
    "  a = a+a;"
    "}");
  CheckResultFromFile();

  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();
  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 3);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("a_out", "3+3");
  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, SingleIfTest) {
  ParseC(
    "void test(int a, int b) {"
    "  if (a)"
    "    b = 2;"
    "}");
  CheckResultFromFile();


  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();

  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 3);
  test->setUnsignedIntegerInput("b_in", 7);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("b_out", "2");

  test->reset();
  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 0);
  test->setUnsignedIntegerInput("b_in", 7);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("b_out", "7");

  test->endStimulusProcess();

  saveTestOperator();
}


TEST_F(SimpleVHDLGeneratorTest, TernaryOperatorTest) {
  ParseC(
    "void test(int a, int b, int c) {"
    "  /*a = 42;*/"
    "  b = 1;"
    "  c = (a > 0 ? a : b);"
    "}");
  CheckResultFromFile();


  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();

  test->waitUntilReady();
  test->startDataInput();
  test->setSignedIntegerInput("a_in", 42);
  test->endDataInput();
  test->waitForAndCheckSignedIntegerResult("b_out", "1");
  test->waitForAndCheckSignedIntegerResult("c_out", "42");

  test->reset();
  test->waitUntilReady();
  test->startDataInput();
  test->setSignedIntegerInput("a_in", -42);
  test->endDataInput();
  test->waitForAndCheckSignedIntegerResult("b_out", "1");
  test->waitForAndCheckSignedIntegerResult("c_out", "1");

  test->endStimulusProcess();

  saveTestOperator();
}


/*
// not implemented
TEST_F(SimpleVHDLGeneratorTest, LocalVariableTest) {
  ParseC(
    "int test(int a) {"
    "  int x = 7;"
    "  int y = 42;"
    "  return (a > 0 ? x : y);"
    "}");
  CheckResultFromFile();
}
*/


TEST_F(SimpleVHDLGeneratorTest, SelectOperationTest) {
  ParseC(
    "#define SGN(x) (x >= 0.0 ? 1.0 : (x == 0.0 ? 0.0 : -1.0))\n"
    "double test(double a) {"
    "  return SGN(a);"
    "}");
  CheckResultFromFile();

  //TODO add VHDL test bench
}


TEST_F(SimpleVHDLGeneratorTest, IntegerExtensionTest) {
  ParseC(
    "unsigned int test(unsigned short a) {"
    "  return a;"
    "}");
  CheckResultFromFile();

  //TODO add VHDL test bench
}


class ReconOSVHDLGeneratorTest : public SimpleVHDLGeneratorTest {

};


TEST_F(ReconOSVHDLGeneratorTest, ReturnValueTest) {
  ParseC(
    "double test(double a) {"
    "  return a+2;"
    "}");
  GenerateCode();
  ::Operator* calculation = getGeneratedOperator();

  boost::scoped_ptr<ReconOSOperator> r_op(new ReconOSOperator());
  r_op->setName("ReturnValueReconOS");
  r_op->setCalculation(calculation);
  r_op->instantiateCalculation();

  r_op->addInitialState();
  r_op->addThreadExitState();

  ChannelP a_in = Channel::make_component_input(calculation, "a_in");
  ChannelP result_out = Channel::make_component_output(calculation, "return");
  r_op->readMbox("READ_A", 0, a_in);
  r_op->writeMbox("WRITE_RESULT", 1, result_out);

  r_op->addAckState();

  saveOperator("ReturnValueReconOS.vhdl", r_op.get());
}
