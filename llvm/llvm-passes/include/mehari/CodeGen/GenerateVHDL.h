#ifndef GENERAGE_VHDL_H
#define GENERAGE_VHDL_H

#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include "mehari/CodeGen/Channel.h"
#include "mehari/CodeGen/ValueStorage.h"
#include "mehari/CodeGen/ReconOSOperator.h"

#include "Operator.hpp"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

class VHDLBackend : public CodeGeneratorBackend, private PhiNodeSink {
  std::string name;

  UniqueNameSource instanceNameGenerator;
  UniqueNameSet usedVariableNames;

  SimpleCCodeGenerator* generator;
  std::ostream* stream;

  boost::scoped_ptr<MyOperator> op;
  boost::scoped_ptr<ReconOSOperator> r_op;
  boost::scoped_ptr<class ValueStorageFactory> vs_factory;
  boost::scoped_ptr<class ReadySignals> ready_signals;
  std::ostringstream interface_ccode;

  bool generateForTest;
public:
  VHDLBackend(const std::string& name);

  MyOperator* getOperator();
  ReconOSOperator* getReconOSOperator();
  std::string getInterfaceCode();
  VHDLBackend* setTestMode();

  void init(SimpleCCodeGenerator* generator, std::ostream& stream);

  void generateStore(Value *op1, Value *op2);
  void generateBinaryOperator(std::string tmpVar, Value *op1, Value *op2, unsigned opcode);
  void generateCall(std::string funcName, std::string tmpVar, std::vector<Value*> args);
  void generateVoidCall(std::string funcName, std::vector<Value*> args);
  void generateComparison(std::string tmpVar, Value *op1, Value *op2, FCmpInst::Predicate comparePredicate);
  void generateIntegerExtension(std::string tmpVar, Value *op);
  void generateSelect(std::string tmpVar, Value *condition, Value *targetTrue, Value *targetFalse);
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
  void addDataDependency(Value *valueFromOtherThread, const std::string& isSavedHere);

private:
  boost::shared_ptr<Channel> getChannel(Value* addr, ChannelDirection::Direction direction);
  boost::shared_ptr<Channel> createChannel(Value* addr, const std::string& name, ChannelDirection::Direction direction);

  void generatePhiNode(ValueStorageP target, Value* condition, ValueStorageP trueValue, ValueStorageP falseValue);

  ValueStorageP remember(ValueStorageP value);

  ChannelP read(ValueStorageP value);

  void mboxGet(unsigned int mbox, ChannelP channel_of_op, ValueStorageP value);
  void mboxPut(unsigned int mbox, ChannelP channel_of_op, ValueStorageP value);
  void mboxGetWithoutInterface(unsigned int mbox, ChannelP channel_of_op);
  void mboxPutWithoutInterface(unsigned int mbox, ChannelP channel_of_op);
};

#endif /*GENERAGE_VHDL_H*/
