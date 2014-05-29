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

  typedef struct {
    std::string name;
    std::string type;
    unsigned int numElem;
  } GlobalArrayVariable;

  void createCCode(std::ostream& stream, Function &func, std::vector<Instruction*> &instructions);
  void createExternArray(std::ostream& stream, GlobalArrayVariable &globVar);

  inline std::string createCCode(Function &func, std::vector<Instruction*> &instructions) {
    std::stringstream stream;
    createCCode(stream, func, instructions);
    return stream.str();
  }

  std::string getOperandString(Value* addr);

  std::map<Value*, std::string>& getVariables();
  std::string getDataDependencyOrDefault(std::string opString, std::string defaultValue);

private:
  std::map<Value*, std::string> variables;
  std::map<std::string, std::vector<std::string> > tmpVariables;
  std::map<std::string, std::string> dataDependencies;
  UniqueNameSource tmpVarNameGenerator;

  CodeGeneratorBackend* backend;

  void resetVariables();

  void extractFunctionParameters(Function &func);

  void addVariable(Value *addr, std::string name);
  void addVariable(Value *addr, std::string name, std::string index);
  void copyVariable(Value *source, Value *target);
  void addIndexToVariable(Value *source, Value *target, std::string index);

  std::string getDatatype(Value *addr);
  std::string getDatatype(Type *type);

  std::string createTemporaryVariable(Value *addr, std::string datatype);
  std::string getOrCreateTemporaryVariable(Value *addr);
};

class CodeGeneratorBackend {
public:
  virtual void init(SimpleCCodeGenerator* generator, std::ostream& stream) =0;

  virtual std::string generateBranchLabel(Value *target) =0;
  virtual void generateStore(Value *op1, Value *op2) =0;
  virtual void generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, unsigned opcode) =0;
  virtual void generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args) =0;
  virtual void generateVoidCall(std::string funcName, std::vector<Value*> args) =0;
  virtual void generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate) =0;
  virtual void generateIntegerExtension(std::string tmpVar, Value *op) =0;
  virtual void generatePhiNodeAssignment(std::string tmpVar, Value *op) =0;
  virtual void generateUnconditionalBranch(std::string label) =0;
  virtual void generateConditionalBranch(Value *condition, std::string label1, std::string label2) =0;
  virtual void generateReturn(Value *retVal) =0;
  virtual void generateVariableDeclarations(const std::map<std::string, std::vector<std::string> >& tmpVariables) =0;
  virtual void generateBranchTargetIfNecessary(llvm::Instruction* instr) =0;
  virtual void generateEndOfMethod();
};

class CCodeBackend : public CodeGeneratorBackend {
  UniqueNameSource branchLabelNameGenerator;
  std::map<Value*, std::vector<std::string> > branchLabels;

  std::stringstream declarations, ccode;
  std::ostream* output_stream;

  SimpleCCodeGenerator* generator;
public:
  CCodeBackend();

  void init(SimpleCCodeGenerator* generator, std::ostream& stream);

  std::string generateBranchLabel(Value *target);
  void generateStore(Value *op1, Value *op2);
  void generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, unsigned opcode);
  void generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args);
  void generateVoidCall(std::string funcName, std::vector<Value*> args);
  void generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate);
  void generateIntegerExtension(std::string tmpVar, Value *op);
  void generatePhiNodeAssignment(std::string tmpVar, Value *op);
  void generateUnconditionalBranch(std::string label);
  void generateConditionalBranch(Value *condition, std::string label1, std::string label2);
  void generateReturn(Value *retVal);
  void generateVariableDeclarations(const std::map<std::string, std::vector<std::string> >& tmpVariables);
  void generateBranchTargetIfNecessary(llvm::Instruction* instr);
  void generateEndOfMethod();

private:
  std::string getOperandString(Value* addr);
};

#endif /*SIMPLE_CCODE_GENERATOR_H*/
