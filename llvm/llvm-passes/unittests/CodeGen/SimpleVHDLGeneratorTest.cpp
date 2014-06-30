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

  ReconOSOperator* getGeneratedReconOSOperator() {
    assert(((VHDLBackend*)backend)->getReconOSOperator());
    return ((VHDLBackend*)backend)->getReconOSOperator();
  }

  std::string getInterfaceCode() {
    return ((VHDLBackend*)backend)->getInterfaceCode();
  }

  std::string CheckReconOSOperator(const std::string &ExpectedResult, const std::string &ExpectedResultFile = "") {
    std::ostringstream CodeGenResult;
    getGeneratedReconOSOperator()->outputVHDL(CodeGenResult, "reconos");

    if (ExpectedResult != CodeGenResult.str()) {
      if (ExpectedResultFile.empty()) {
        remove("expected-reconos");
        writeFile("expected-reconos", ExpectedResult);
      } else
        link(ExpectedResultFile, "expected-reconos");
      writeFile("actual-reconos", CodeGenResult.str());
    }

    // compare results
    EXPECT_EQ(ExpectedResult, CodeGenResult.str());

    return CodeGenResult.str();
  }

  std::string CheckReconOSOperatorFromFile(const std::string& filename) {
    return CheckReconOSOperator(fromFile(filename), "CodeGen_data/" + filename);
  }

  std::string CheckReconOSOperatorFromFile() {
    return CheckReconOSOperatorFromFile(getCurrentTestName() + "-ReconOS.vhdl");
  }

public:
  SimpleVHDLGeneratorTest() : testOp(NULL) { }

private:
  TestOperator *testOp;

  CodeGeneratorBackend* createBackend() {
    backend = (new VHDLBackend("test"))->setTestMode();
    return backend;
  }

  void configureCodeGenerator() {
    codeGen->setIgnoreDataDependencies(true);
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


TEST_F(SimpleVHDLGeneratorTest, OrTest) {
  ParseC(
    "void test(int a, int b) {"
    "  b = a | 6;"
    "}");
  CheckResultFromFile();


  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();

  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 32+4+1);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("b_out", "39");

  test->reset();
  test->waitUntilReady();
  test->startDataInput();
  test->setUnsignedIntegerInput("a_in", 2);
  test->endDataInput();
  test->waitForAndCheckUnsignedIntegerResult("b_out", "6");

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


  TestOperator* test = makeTestOperator();

  test->beginStimulusProcess();

  test->waitUntilReady();
  test->startDataInput();
  test->setSignedIntegerInput("a_in", 42);
  test->endDataInput();
  test->waitForAndCheckFloatResult("return", "1.0");

  test->reset();
  test->waitUntilReady();
  test->startDataInput();
  test->setSignedIntegerInput("a_in", -42);
  test->endDataInput();
  test->waitForAndCheckFloatResult("return", "-1.0");

  // This test case cannot work due to a but in the SGN code which is copied from IPANEMA.
  /*test->reset();
  test->waitUntilReady();
  test->startDataInput();
  test->setSignedIntegerInput("a_in", 0.0);
  test->endDataInput();
  test->waitForAndCheckFloatResult("return", "-0.0");*/

  test->endStimulusProcess();

  saveTestOperator();
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

  boost::scoped_ptr<BasicReconOSOperator> r_op(new BasicReconOSOperator());
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


TEST_F(ReconOSVHDLGeneratorTest, GlobalArrayTest) {
  ParseC(
    "double x[7];"
    "double test(double a) {"
    "  x[2] = a;"
    "  return x[1];"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_real(&mbox_start, a);\n"
    "x[2] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, x[1]);\n"
    "return mbox_get_real(&mbox_stop);\n",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, DoubleCommunicationTest) {
  ParseC(
    "void _put_real(unsigned int, double);"
    "double _get_real(unsigned int);"
    "void test(double a) {"
    "  _put_real(1, a);"
    "  a = _get_real(0);"
    "}");
  std::string vhdl_code = CheckResultFromFile();
  std::string reconos_code = CheckReconOSOperatorFromFile();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", vhdl_code);
  writeFile(getCurrentTestName() + "/reconos.vhdl", reconos_code);

  EXPECT_EQ(
    "mbox_put_real(&mbox_start, a);\n"
    "a = mbox_get_real(&mbox_stop);\n",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, IntCommunicationTest) {
  ParseC(
    "void _put_int(unsigned int, unsigned int);"
    "int _get_int(unsigned int);"
    "void test(int a) {"
    "  _put_int(1, a);"
    "  a = _get_int(0);"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_int(&mbox_start, a);\n"
    "a = mbox_get_int(&mbox_stop);\n",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, BoolCommunicationTest) {
  ParseC(
    "void _put_bool(unsigned int, unsigned int);"
    "int _get_bool(unsigned int);"
    "void test(int a) {"
    "  _put_bool(1, a);"
    "  a = _get_bool(0);"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_int(&mbox_start, a);\n"
    "a = mbox_get_int(&mbox_stop);\n",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, SemaphoreTest) {
  ParseC(
    "void _sem_wait(unsigned int);"
    "void _sem_post(unsigned int);"
    "void test(void) {"
    "  _sem_wait(23);"
    "  _sem_post(42);"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, StatusPointerTest) {
  ParseC(
    "void test(int *status) {"
    "  *status |= 7;"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_int(&mbox_start, *status);\n*status = mbox_get_int(&mbox_stop);\n",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, DCMotorTest) {
  ParseC(
    "double Vp[100], Vu[100], Vq[100], Vx[100], Vy[100];"
    "int Vs[100];"
    "void test(double* x, double t) {"
    "  Vp[14] = Vp[0];"
    "  Vp[15] = Vp[1];"
    "  Vu[4] = 0.00000000000000000e+000;"
    "  Vq[14] = t*Vp[10]+Vp[11]/3.60000000000000000e+002;"
    "  Vx[0] = x[0];"
    "  Vx[1] = x[1];"
    "  Vx[2] = x[2];"
    "  Vs[4] = Vq[14] > Vp[12];"
    "  x[0] = Vx[0];"
    "  Vy[4] = Vp[10]*Vx[0];"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_real(&mbox_start, Vp[0]);\n"
    "Vp[14] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, Vp[1]);\n"
    "Vp[15] = mbox_get_real(&mbox_stop);\n"
    "Vu[4] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, t);\n"
    "mbox_put_real(&mbox_start, Vp[10]);\n"
    "mbox_put_real(&mbox_start, Vp[11]);\n"
    "Vq[14] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, x[0]);\n"
    "Vx[0] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, x[1]);\n"
    "Vx[1] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, x[2]);\n"
    "Vx[2] = mbox_get_real(&mbox_stop);\n"
    "mbox_put_real(&mbox_start, Vp[12]);\n"
    "Vs[4] = mbox_get_int(&mbox_stop);\n"
    "x[0] = mbox_get_real(&mbox_stop);\n"
    "Vy[4] = mbox_get_real(&mbox_stop);\n",
    getInterfaceCode());
}

/*
TEST_F(ReconOSVHDLGeneratorTest, NestedIf) {
  ParseC(
    "void test(double* x, int* Vs, double t) {"
    "  if (!Vs[6]) if (Vs[7])         x[65] = x[63]*0.00000000000000000e+000;"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_int(&mbox_start, Vs[6]);\n"
    "mbox_put_int(&mbox_start, Vs[7]);\n"
    "mbox_put_real(&mbox_start, Vq[63]);\n"
    "mbox_put_real(&mbox_start, Vq[65]);\n"
    "Vq[65] = mbox_get_real(&mbox_stop);\n",
    getInterfaceCode());
}


TEST_F(ReconOSVHDLGeneratorTest, TwoMassesBouncingTest) {
  ParseC(
    "double Vp[1000], Vu[1000], Vq[1000], Vx[1000], Vy[1000];"
    "int Vs[1000];"
    "void test(double* x, double t) {"
    "  if (Vs[6])         Vq[53] = Vq[52]*0.00000000000000000e+000;"
    "  if (Vs[6])         Vq[64] = Vq[63]*0.00000000000000000e+000;"
    "  if (Vs[6])         Vq[75] = Vq[74]*(1.73205103309698990e-007/Vq[84]);"
    "  if (!Vs[6]) if (Vs[7])         Vq[65] = Vq[63]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (Vs[7])         Vq[76] = Vq[74]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (Vs[7])         Vq[85] = Vq[84]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (!Vs[7]) if (Vs[8])         Vq[54] = Vq[52]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (!Vs[7]) if (Vs[8])         Vq[77] = Vq[74]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (!Vs[7]) if (Vs[8])         Vq[86] = Vq[84]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (!Vs[7]) if (!Vs[8]) if (Vs[9])         Vq[66] = Vq[63]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (!Vs[7]) if (!Vs[8]) if (Vs[9])         Vq[55] = Vq[52]*0.00000000000000000e+000;"
    "  if (!Vs[6]) if (!Vs[7]) if (!Vs[8]) if (Vs[9])         Vq[87] = Vq[84]*(1.73205103309698990e-007/Vq[74]);"
    "  if (Vs[14])         Vq[103] = Vq[102]*0.00000000000000000e+000;"
    "  if (Vs[14])         Vq[114] = Vq[113]*0.00000000000000000e+000;"
    "  if (Vs[14])         Vq[125] = Vq[124]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (Vs[15])         Vq[115] = Vq[113]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (Vs[15])         Vq[126] = Vq[124]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (Vs[15])         Vq[135] = Vq[134]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (!Vs[15]) if (Vs[16])         Vq[104] = Vq[102]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (!Vs[15]) if (Vs[16])         Vq[127] = Vq[124]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (!Vs[15]) if (Vs[16])         Vq[136] = Vq[134]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (!Vs[15]) if (!Vs[16]) if (Vs[17])         Vq[116] = Vq[113]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (!Vs[15]) if (!Vs[16]) if (Vs[17])         Vq[105] = Vq[102]*0.00000000000000000e+000;"
    "  if (!Vs[14]) if (!Vs[15]) if (!Vs[16]) if (Vs[17])         Vq[137] = Vq[134]*0.00000000000000000e+000;"
    "}");
  std::string code = GenerateCode();

  mkdir(getCurrentTestName().c_str(), 0755);
  writeFile(getCurrentTestName() + "/calculation.vhdl", code);

  saveOperator(getCurrentTestName() + "/reconos.vhdl", getGeneratedReconOSOperator());

  EXPECT_EQ(
    "mbox_put_int(&mbox_start, Vs[6]);\n"
    "mbox_put_real(&mbox_start, Vq[52]);\n"
    "Vq[53] = mbox_get_real(&mbox_stop);\n",
    getInterfaceCode());
}
*/