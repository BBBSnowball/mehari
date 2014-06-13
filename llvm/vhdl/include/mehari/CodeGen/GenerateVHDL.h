#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include "Operator.hpp"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

class MyOperator : public ::Operator {
public:
  template<typename T>
  inline void addCode(const T& x) {
    vhdl << x;
  }

  void licence(std::ostream& o, std::string authorsyears);

  void stdLibs(std::ostream& o);
};

namespace ChannelDirection {
  enum Direction { IN = 0x1, OUT = 0x2, CONSTANT_OUT=(OUT|0x4), INOUT = (IN|OUT) };

  // both directions include either IN or OUT, e.g.:
  // IN, IN   -> true
  // OUT, OUT -> true
  // OUT, IN|OUT -> true
  // IN|OUT, OUT -> true
  inline static bool matching_direction(Direction a, Direction b) {
    return ((a&0x3) & (b&0x3)) != 0;
  }
}

class Channel;

typedef boost::shared_ptr<class ValueStorage> ValueStorageP;

class PhiNodeSink {
public:
  virtual void generatePhiNode(ValueStorageP target, Value* condition, ValueStorageP trueValue, ValueStorageP falseValue) =0;
};

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

