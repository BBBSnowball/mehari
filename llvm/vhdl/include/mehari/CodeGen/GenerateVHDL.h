#ifndef GENERAGE_VHDL_H
#define GENERAGE_VHDL_H

#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include "mehari/CodeGen/Channel.h"
#include "mehari/CodeGen/ValueStorage.h"

#include "Operator.hpp"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

class VHDLBackend : public CodeGeneratorBackend, private PhiNodeSink {
  UniqueNameSource instanceNameGenerator;
  UniqueNameSet usedVariableNames;

  SimpleCCodeGenerator* generator;
  std::ostream* stream;

  boost::scoped_ptr<MyOperator> op;
  boost::scoped_ptr<class ValueStorageFactory> vs_factory;
  boost::scoped_ptr<class ReadySignals> ready_signals;
public:
  VHDLBackend();

  MyOperator* getOperator();

  void init(SimpleCCodeGenerator* generator, std::ostream& stream);

  void generateStore(Value *op1, Value *op2);
  void generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, unsigned opcode);
  void generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args);
  void generateVoidCall(std::string funcName, std::vector<Value*> args);
  void generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate);
  void generateIntegerExtension(std::string tmpVar, Value *op);
  void generatePhiNodeAssignment(std::string tmpVar, Value *op);
  void generateUnconditionalBranch(Instruction *target);
  void generateConditionalBranch(Value *condition, Instruction *targetTrue, Instruction *targetFalse);
  void generateReturn(Value *retVal);
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
  boost::shared_ptr<Channel> getChannel(Value* addr, ChannelDirection::Direction direction);
  boost::shared_ptr<Channel> createChannel(Value* addr, const std::string& name, ChannelDirection::Direction direction);

  void generatePhiNode(ValueStorageP target, Value* condition, ValueStorageP trueValue, ValueStorageP falseValue);

  ValueStorageP remember(ValueStorageP value);
};

#endif /*GENERAGE_VHDL_H*/
