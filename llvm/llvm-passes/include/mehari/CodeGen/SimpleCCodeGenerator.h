#ifndef SIMPLE_CCODE_GENERATOR_H
#define SIMPLE_CCODE_GENERATOR_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "mehari/utils/UniqueNameSource.h"

using namespace llvm;

class CodeGeneratorBackend;
class CCodeBackend;

class SimpleCCodeGenerator {

public:
  SimpleCCodeGenerator(CodeGeneratorBackend* backend = NULL);
  ~SimpleCCodeGenerator();

  void setIgnoreDataDependencies(bool ignoreThem);

  typedef struct {
    std::string name;
    std::string type;
    unsigned int numElem;
  } GlobalArrayVariable;

  void createCCode(std::ostream& stream, Function &func, const std::vector<Instruction*> &instructions);
  void createExternArray(std::ostream& stream, GlobalArrayVariable &globVar);

  inline std::string createCCode(Function &func, const std::vector<Instruction*> &instructions) {
    std::stringstream stream;
    createCCode(stream, func, instructions);
    return stream.str();
  }

  std::string getDataDependencyOrDefault(std::string opString, std::string defaultValue);

private:
  std::map<std::string, std::string> dataDependencies;
  bool ignoreDataDependencies;

  CodeGeneratorBackend* backend;

  void resetVariables();

  void extractFunctionParameters(Function &func);
};

class CodeGeneratorBackend {
public:
  virtual ~CodeGeneratorBackend();

  virtual void init(SimpleCCodeGenerator* generator, std::ostream& stream) =0;

  virtual void generateStore(Value *op1, Value *op2) =0;
  virtual void generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, unsigned opcode) =0;
  virtual void generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args) =0;
  virtual void generateVoidCall(std::string funcName, std::vector<Value*> args) =0;
  virtual void generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate) =0;
  virtual void generateIntegerExtension(std::string tmpVar, Value *op) =0;
  virtual void generateSelect(std::string tmpVar, Value *condition, Value *targetTrue, Value *targetFalse) =0;
  virtual void generatePhiNodeAssignment(std::string tmpVar, Value *op) =0;
  virtual void generateUnconditionalBranch(Instruction *target) =0;
  virtual void generateConditionalBranch(Value *condition, Instruction *targetTrue, Instruction *targetFalse) =0;
  virtual void generateReturn(Value *retVal) =0;
  virtual void generateBranchTargetIfNecessary(llvm::Instruction* instr) =0;
  virtual void generateEndOfMethod();

  virtual std::string createTemporaryVariable(Value *addr) =0;
  virtual std::string getOrCreateTemporaryVariable(Value *addr) =0;
  virtual void addParameter(Value *addr, std::string name) =0;
  virtual void addVariable(Value *addr, std::string name) =0;
  virtual void addVariable(Value *addr, std::string name, std::string index) =0;
  virtual void copyVariable(Value *source, Value *target) =0;
  virtual void addIndexToVariable(Value *source, Value *target, std::string index) =0;
  virtual void addDataDependency(Value *valueFromOtherThread, const std::string& isSavedHere);
};

class CCodeBackend : public CodeGeneratorBackend {
  UniqueNameSource branchLabelNameGenerator;
  std::map<Value*, std::vector<std::string> > branchLabels;
  std::map<Value*, std::string> variables;
  std::map<std::string, std::vector<std::string> > tmpVariables;
  UniqueNameSource tmpVarNameGenerator;

  std::stringstream declarations, ccode;
  std::ostream* output_stream;

  SimpleCCodeGenerator* generator;
public:
  CCodeBackend();
  ~CCodeBackend();

  void init(SimpleCCodeGenerator* generator, std::ostream& stream);

  std::string generateBranchLabel(Value *target);
  void generateStore(Value *op1, Value *op2);
  void generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, unsigned opcode);
  void generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args);
  void generateVoidCall(std::string funcName, std::vector<Value*> args);
  void generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate);
  void generateIntegerExtension(std::string tmpVar, Value *op);
  void generateSelect(std::string tmpVar, Value *condition, Value *targetTrue, Value *targetFalse);
  void generatePhiNodeAssignment(std::string tmpVar, Value *op);
  void generateUnconditionalBranch(std::string label);
  void generateConditionalBranch(Value *condition, std::string label1, std::string label2);
  void generateUnconditionalBranch(Instruction *target);
  void generateConditionalBranch(Value *condition, Instruction *targetTrue, Instruction *targetFalse);
  void generateReturn(Value *retVal);
  void generateVariableDeclarations();
  void generateBranchTargetIfNecessary(llvm::Instruction* instr);
  void generateEndOfMethod();

  std::string createTemporaryVariable(Value *addr);
  std::string getOrCreateTemporaryVariable(Value *addr);
  void addParameter(Value *addr, std::string name);
  void addVariable(Value *addr, std::string name);
  void addVariable(Value *addr, std::string name, std::string index);
  void copyVariable(Value *source, Value *target);
  void addIndexToVariable(Value *source, Value *target, std::string index);

private:
  std::string getOperandString(Value* addr);

  std::string getDatatype(Value *addr);
  std::string getDatatype(Type *type);
};

#endif /*SIMPLE_CCODE_GENERATOR_H*/
