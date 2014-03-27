#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"

#include "gtest/gtest.h"

#include "mehari/Analysis/InstructionDependencyAnalysis.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <vector>
#include <string>
#include <sstream>


using namespace llvm;

namespace {

class InstructionDependencyAnalysisTest : public testing::Test {

protected:
  
  void ParseAssembly(const char *Assembly) {
    M.reset(new Module("Module", getGlobalContext()));

    SMDiagnostic Error;
    bool Parsed = ParseAssemblyString(Assembly, M.get(), Error, M->getContext()) == M.get();

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


  InstructionDependencyList ParseResults(std::string &result) {
    InstructionDependencyList parsedResult;
    std::istringstream iss(result);
    for (std::string entry; std::getline(iss, entry); ) {
        InstructionDependencies tmpDep;
        std::vector<std::string> dependencies;
        boost::algorithm::split(dependencies, entry, boost::algorithm::is_any_of(" "));
        for (std::vector<std::string>::iterator it = dependencies.begin(); it != dependencies.end(); ++it)
          if ((*it) != "-")
            tmpDep.push_back(atoi((*it).c_str()));
        parsedResult.push_back(tmpDep);
    }
    return parsedResult;
  }


  void CheckResult(InstructionDependencyList ExpectedResult) {

    static char ID;

    class CheckResultPass : public FunctionPass {
     public:
      CheckResultPass(Function &F, InstructionDependencyList ExpectedResult)
          : FunctionPass(ID), ExpectedResult(ExpectedResult) {}

      static int initialize() {
        PassInfo *PI = new PassInfo("CheckResult testing pass",
                                    "", &ID, 0, true, true);
        PassRegistry::getPassRegistry()->registerPass(*PI, false);
        initializeInstructionDependencyAnalysisPass(*PassRegistry::getPassRegistry());
        return 0;
      }

      void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesAll();
        AU.addRequiredTransitive<InstructionDependencyAnalysis>();
      }

      bool runOnFunction(Function &F) {
        if (!F.hasName() || F.getName() != "test")
          return false;

        // create worklist containing all instructions of the function
        std::vector<Instruction*> worklist;
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) 
          worklist.push_back(&*I);

        // run instruction dependency analysis
        InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>();
        InstructionDependencyList AnalysisResults = IDA->getDependencies(worklist);

        // DEBUG: print expected and analysis results
        if (false) {
          std::cout << "EXPECTED RESULTS:\n";
          int index = 0;
          for (InstructionDependencyList::iterator instrDepIt = ExpectedResult.begin(); 
                instrDepIt != ExpectedResult.end(); ++instrDepIt, index++) {
            std::cout << index << ":";
            for (InstructionDependencies::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
              std::cout << " " << int(*depNumIt);
            }
            std::cout << "\n";
          }
          std::cout << "ANALYSIS RESULTS:\n";
          index = 0;
          for (InstructionDependencyList::iterator instrDepIt = AnalysisResults.begin(); 
                instrDepIt != AnalysisResults.end(); ++instrDepIt, index++) {
            std::cout << index << ":";
            for (InstructionDependencies::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
              std::cout << " " << int(*depNumIt);
            }
            std::cout << "\n";
          }
        }

        // compare the analysis results with the expected result
        EXPECT_EQ(ExpectedResult.size(), AnalysisResults.size());
        for (int i=0; i<AnalysisResults.size(); i++) {
          EXPECT_EQ(ExpectedResult[i].size(), AnalysisResults[i].size());
          for (int j=0; j<AnalysisResults[i].size(); j++) {
            EXPECT_EQ(ExpectedResult[i][j], AnalysisResults[i][j]);
          }
        }

        return false;
      }
      
      InstructionDependencyList ExpectedResult;
    };

    static int initialize = CheckResultPass::initialize();
    (void)initialize;

    CheckResultPass *P = new CheckResultPass(*F, ExpectedResult);
    PassManager PM;
    PM.add(P);
    PM.run(*M);
  }

private:
  OwningPtr<Module> M;
  Function *F;

}; // end class InstructionDependencyAnalysisTest

} // end anonymous namespace


// int a, b;
// a = 1;
// b = a;
TEST_F(InstructionDependencyAnalysisTest, AssignmentTest) {
  ParseAssembly(
    "define void @test() #0 {\n"
    "  %a = alloca i32, align 4\n"
    "  %b = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  %1 = load i32* %a, align 4\n"
    "  store i32 %1, i32* %b, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "0\n"
    /*3:*/ "0 2\n"
    /*4:*/ "1 3\n"
    /*5:*/ "-\n";
  CheckResult(ParseResults(result));
}

// int a, b;
// a = 1;
// b = 2-1+a;
TEST_F(InstructionDependencyAnalysisTest, CalculationTest) {
  ParseAssembly(
    "define void @test() #0 {\n"
    "  %a = alloca i32, align 4\n"
    "  %b = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  %1 = load i32* %a, align 4\n"
    "  %add = add nsw i32 1, %1\n"
    "  store i32 %add, i32* %b, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "0\n"
    /*3:*/ "0 2\n"
    /*4:*/ "3\n"
    /*5:*/ "1 4\n"
    /*6:*/ "-\n";
  CheckResult(ParseResults(result));
}
