#include "gtest/gtest.h"

#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include <boost/scoped_ptr.hpp>


class CodeGeneratorTest : public testing::Test {

protected:
  
  void ParseAssembly(const char *Assembly);
  void ParseC(const char *Code);

  std::string GenerateCode();

  void CheckResult(const std::string &ExpectedResult, bool printResults = false,
    const std::string &ExpectedResultFile = "");
  void CheckResultFromFile(const std::string& filename, bool printResults = false);
  void CheckResultFromFile();

  static void link(const std::string& source, const std::string& target);

  std::string fromFile(const std::string& filename);
  std::string readFile(const std::string& filename);
  void writeFile(const std::string& filename, const std::string& contents);

  std::string getCurrentTestName();

  SimpleCCodeGenerator* getCodeGenerator();

public:
  CodeGeneratorTest();

protected:
  std::string assembly;
  Module *M;
  Function *F;
  CodeGeneratorBackend *backend;
  boost::scoped_ptr<SimpleCCodeGenerator> codeGen;

protected:
  virtual CodeGeneratorBackend* createBackend() =0;
};
