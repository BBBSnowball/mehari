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
#include <algorithm>


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


  InstructionDependencyNumbersList ParseResults(std::string &result) {
    InstructionDependencyNumbersList parsedResult;
    std::istringstream iss(result);
    for (std::string entry; std::getline(iss, entry); ) {
        InstructionDependencyNumbers tmpDep;
        std::vector<std::string> dependencies;
        boost::algorithm::split(dependencies, entry, boost::algorithm::is_any_of(" "));
        for (std::vector<std::string>::iterator it = dependencies.begin(); it != dependencies.end(); ++it)
          if ((*it) != "-")
            tmpDep.push_back(atoi((*it).c_str()));
        parsedResult.push_back(tmpDep);
    }
    return parsedResult;
  }


  void CheckResult(InstructionDependencyNumbersList ExpectedResult) {

    static char ID;

    class CheckResultPass : public FunctionPass {
     public:
      CheckResultPass(Function &F, InstructionDependencyNumbersList ExpectedResult)
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

        // run instruction dependency analysis
        InstructionDependencyAnalysis *IDA = &getAnalysis<InstructionDependencyAnalysis>();
        InstructionDependencyNumbersList AnalysisResult = IDA->getDependencyNumbers(F);

        // DEBUG: print expected and analysis results
        if (false) {
          std::cout << "EXPECTED RESULTS:\n";
          int index = 0;
          for (InstructionDependencyNumbersList::iterator instrDepIt = ExpectedResult.begin(); 
                instrDepIt != ExpectedResult.end(); ++instrDepIt, index++) {
            std::cout << index << ":";
            for (InstructionDependencyNumbers::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
              std::cout << " " << int(*depNumIt);
            }
            std::cout << "\n";
          }
          std::cout << "ANALYSIS RESULTS:\n";
          index = 0;
          for (InstructionDependencyNumbersList::iterator instrDepIt = AnalysisResult.begin(); 
                instrDepIt != AnalysisResult.end(); ++instrDepIt, index++) {
            std::cout << index << ":";
            for (InstructionDependencyNumbers::iterator depNumIt = instrDepIt->begin(); depNumIt != instrDepIt->end(); ++depNumIt) {
              std::cout << " " << int(*depNumIt);
            }
            std::cout << "\n";
          }
        }

        // compare the analysis results with the expected result
        EXPECT_EQ(ExpectedResult.size(), AnalysisResult.size());
        for (unsigned int i=0; i<std::min(AnalysisResult.size(), ExpectedResult.size()); i++) {
          EXPECT_EQ(ExpectedResult[i].size(), AnalysisResult[i].size());
          for (unsigned int j=0; j<std::min(AnalysisResult[i].size(), ExpectedResult[i].size()); j++) {
            EXPECT_EQ(ExpectedResult[i][j], AnalysisResult[i][j]);
          }
        }

        return false;
      }
      
      InstructionDependencyNumbersList ExpectedResult;
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


TEST_F(InstructionDependencyAnalysisTest, EmptyFunctionTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, AllocaStoreTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "0\n"
    /*2:*/ "1\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, AllocaStoreLoadTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  %0 = load i32* %a, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "0\n"
    /*2:*/ "0 1\n"
    /*3:*/ "2\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, MultipleStoreLoadTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  store i32 2, i32* %a, align 4\n"
    "  %0 = load i32* %a, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "0\n"
    /*2:*/ "0\n"
    /*3:*/ "0 1 2\n"
    /*4:*/ "3\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, ArrayAccessTest) {
  ParseAssembly(
    "define void @test(double* %param) {\n"
    "entry:\n"
    "  %a = alloca [3 x i32], align 4\n"
    "  %arrayidx = getelementptr inbounds [3 x i32]* %a, i32 0, i64 0\n"
    "  store i32 42, i32* %arrayidx, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "0\n"
    /*2:*/ "1\n"
    /*3:*/ "2\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, GlobalArrayAccessTest) {
  ParseAssembly(
    "@b = common global [3 x i32] zeroinitializer, align 4\n"
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  %0 = load i32* getelementptr inbounds ([3 x i32]* @b, i32 0, i64 0), align 4\n"
    "  store i32 %0, i32* %a, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "0 1\n"
    /*3:*/ "2\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, ArrayParamTest) {
  ParseAssembly(
    "define void @test(double* %param) {\n"
    "entry:\n"
    "%param.addr = alloca double*, align 8\n"
    "store double* %param, double** %param.addr, align 8"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "0\n"
    /*2:*/ "1\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, CalculationTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  %b = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  store i32 2, i32* %b, align 4\n"
    "  %0 = load i32* %a, align 4\n"
    "  %1 = load i32* %b, align 4\n"
    "  %add = add nsw i32 %0, %1\n"
    "  store i32 %add, i32* %a, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "0\n"
    /*3:*/ "1\n"
    /*4:*/ "0 2\n"
    /*5:*/ "1 3\n"
    /*6:*/ "4 5\n"
    /*7:*/ "0 6\n"
    /*8:*/ "7\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, DoubleComparisonTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca double, align 8\n"
    "  store double 1.0, double* %a, align 8\n"
    "  %b = alloca double, align 8\n"
    "  store double 2.0, double* %b, align 8\n"
    "  %0 = load double* %a, align 8\n"
    "  %1 = load double* %b, align 8\n"
    "  %cmp = fcmp ogt double %0, %1\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "0\n"
    /*2:*/ "-\n"
    /*3:*/ "2\n"
    /*4:*/ "0 1\n"
    /*5:*/ "2 3\n"
    /*6:*/ "4 5\n"
    /*7:*/ "6\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, ExtendOperandTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  %b = alloca i32, align 4\n"
    "  %c = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  store i32 2, i32* %b, align 4\n"
    "  %0 = load i32* %a, align 4\n"
    "  %1 = load i32* %b, align 4\n"
    "  %cmp = icmp sgt i32 %0, %1\n"
    "  %conv = zext i1 %cmp to i32\n"
    "  store i32 %conv, i32* %c, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "-\n"
    /*3:*/ "0\n"
    /*4:*/ "1\n"
    /*5:*/ "0 3\n"
    /*6:*/ "1 4\n"
    /*7:*/ "5 6\n"
    /*8:*/ "7\n"
    /*9:*/ "2 8\n"
    /*10:*/ "9\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, IfElseTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  %b = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  %0 = load i32* %a, align 4\n"
    "  %tobool = icmp ne i32 %0, 0\n"
    "  br i1 %tobool, label %if.then, label %if.end\n"
    "  if.then:                                          ; preds = %entry\n"
    "    store i32 2, i32* %b, align 4\n"
    "    br label %if.end\n"
    "  if.end:                                           ; preds = %if.then, %entry\n"
    "    ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "0\n"
    /*3:*/ "0 2\n"
    /*4:*/ "3\n"
    /*5:*/ "4\n"
    /*6:*/ "1\n"
    /*7:*/ "6\n"
    /*8:*/ "7\n";
  CheckResult(ParseResults(result));
}


TEST_F(InstructionDependencyAnalysisTest, PhyNodeTest) {
  ParseAssembly(
    "define void @test() {\n"
    "entry:\n"
    "  %a = alloca i32, align 4\n"
    "  %b = alloca i32, align 4\n"
    "  store i32 1, i32* %a, align 4\n"
    "  %0 = load i32* %a, align 4\n"
    "  %tobool = icmp ne i32 %0, 0\n"
    "  br i1 %tobool, label %cond.true, label %cond.false\n"
    "cond.true:                                        ; preds = %entry\n"
    "  %1 = load i32* %a, align 4\n"
    "  br label %cond.end\n"
    "cond.false:                                       ; preds = %entry\n"
    "  br label %cond.end\n"
    "cond.end:                                         ; preds = %cond.false, %cond.true\n"
    "  %cond = phi i32 [ %1, %cond.true ], [ 2, %cond.false ]\n"
    "  store i32 %cond, i32* %b, align 4\n"
    "  ret void\n"
    "}\n");
  std::string result = 
    /*0:*/ "-\n"
    /*1:*/ "-\n"
    /*2:*/ "0\n"
    /*3:*/ "0 2\n"
    /*4:*/ "3\n"
    /*5:*/ "4\n"
    /*6:*/ "0 2\n"
    /*7:*/ "6\n"
    /*8:*/ "-\n"
    /*9:*/ "6\n"
    /*10:*/ "1 9\n"
    /*11:*/ "10\n";
  CheckResult(ParseResults(result));
}
