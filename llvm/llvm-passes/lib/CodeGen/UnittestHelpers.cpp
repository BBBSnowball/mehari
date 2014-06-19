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

#include "mehari/CodeGen/UnittestHelpers.h"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <boost/foreach.hpp>


CodeGeneratorTest::CodeGeneratorTest() { }

SimpleCCodeGenerator* CodeGeneratorTest::getCodeGenerator() {
  if (!codeGen) {
    backend = createBackend();
    codeGen.reset(new SimpleCCodeGenerator(backend));
  }

  return codeGen.get();
}

void CodeGeneratorTest::ParseAssembly(const char *Assembly) {
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


void CodeGeneratorTest::ParseC(const char *Code) {
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


std::string CodeGeneratorTest::GenerateCode() {
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
  return getCodeGenerator()->createCCode(*F, worklist);
}


void CodeGeneratorTest::CheckResult(const std::string &ExpectedResult, bool printResults, const std::string &ExpectedResultFile) {
  std::string CodeGenResult = GenerateCode();

  if (printResults) {
    errs() << "### Expected Result:\n" << ExpectedResult << "\n";
    errs() << "### Generated Result:\n" << CodeGenResult << "\n";
  }

  if (ExpectedResult != CodeGenResult) {
    if (ExpectedResultFile.empty()) {
      remove("expected");
      writeFile("expected", ExpectedResult);
    } else
      link(ExpectedResultFile, "expected");
    writeFile("actual",   CodeGenResult);
  }

  // compare results
  EXPECT_EQ(ExpectedResult, CodeGenResult);
}

void CodeGeneratorTest::link(const std::string& source, const std::string& target) {
  std::string command = "ln -sf '" + source + "' '" + target + "'";
  int result = system(command.c_str());
  EXPECT_EQ(result, 0);
}

std::string CodeGeneratorTest::fromFile(const std::string& filename) {
  std::string full_filename = "CodeGen_data/" + filename;
  return readFile(full_filename);
}

std::string CodeGeneratorTest::readFile(const std::string& filename) {
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

void CodeGeneratorTest::writeFile(const std::string& filename, const std::string& contents) {
  std::ofstream file;
  file.open(filename.c_str(), std::ios::out);
  file << contents;
  file.close();
}

void CodeGeneratorTest::CheckResultFromFile(const std::string& filename, bool printResults) {
  CheckResult(fromFile(filename), printResults, "CodeGen_data/" + filename);
}


std::string CodeGeneratorTest::getCurrentTestName() {
  const ::testing::TestInfo* const test_info =
  ::testing::UnitTest::GetInstance()->current_test_info();
  return test_info->name();
}

void CodeGeneratorTest::CheckResultFromFile() { CheckResultFromFile(getCurrentTestName() + ".vhdl"); }
