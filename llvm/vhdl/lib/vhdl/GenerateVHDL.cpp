#include "mehari/CodeGen/GenerateVHDL.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <fstream>

#include <boost/foreach.hpp>

#include "mehari/utils/MapUtils.h"
#include "mehari/utils/StringUtils.h"


using namespace ChannelDirection;

//#define DRY_RUN
#define ENABLE_DEBUG_PRINT


#ifdef ENABLE_DEBUG_PRINT
# define debug_print(x) (std::cout << x << std::endl)
#else
# define debug_print(x)
#endif

#ifdef DRY_RUN
# define return_if_dry_run() return
# define return_value_if_dry_run(x) return (x)
#else
# define return_if_dry_run()
# define return_value_if_dry_run(x)
#endif


void MyOperator::licence(std::ostream& o, std::string authorsyears) {
  o<<"-- generated by Mehari"<<endl;
}

template<typename T>
inline MyOperator& operator <<(MyOperator& op, const T& x) {
  op.addCode(x);
  return op;
}

typedef boost::shared_ptr<class Channel> ChannelP;

struct ValueStorage {
  enum Kind {
    FUNCTION_PARAMETER,
    VARIABLE,
    GLOBAL_VARIABLE,
    CHANNEL
  };
  Kind kind;
  std::string name;
  std::vector<std::string> constant_indices;
  Type* type;

  ValueStorage() : type(NULL) { }

  unsigned int width() const {
    unsigned int w = _width();
    assert(w != 0);
    return w;
  }

  unsigned int _width() const {
    return elementType()->getPrimitiveSizeInBits();
  }

  Type* elementType() const {
    //TODO We sometimes have one level of pointers too many, so we remove it here.
    //     However, we should rather fix the problem that causes this.
    Type* type = this->type;
    assert(type);

    if (isa<SequentialType>(type))
      type = type->getSequentialElementType();

    return type;
  }

private:
  ChannelP channel_read, channel_write;

public:
  ChannelP getReadChannel(MyOperator* op);
  ChannelP getWriteChannel(MyOperator* op);

  void initWithChannels(ChannelP channel_read, ChannelP channel_write);

  // Call this, if you write a value to channel_write.
  void replaceBy(ChannelP channel_read) {
    this->channel_read = channel_read;
  }
};
typedef boost::shared_ptr<ValueStorage> ValueStorageP;

std::ostream& operator <<(std::ostream& stream, ValueStorage::Kind kind) {
  switch (kind) {
    case ValueStorage::FUNCTION_PARAMETER:
      return stream << "FUNCTION_PARAMETER";
    case ValueStorage::VARIABLE:
      return stream << "VARIABLE";
    case ValueStorage::GLOBAL_VARIABLE:
      return stream << "GLOBAL_VARIABLE";
    case ValueStorage::CHANNEL:
      return stream << "CHANNEL";
    default:
      return stream << (unsigned)kind;
  }
}

std::ostream& operator <<(std::ostream& stream, const ValueStorage& vs) {
  stream << "ValueStorage(kind = " << vs.kind << ", name = " << vs.name;
  if (!vs.constant_indices.empty()) {
    stream << ", indices = ";
    BOOST_FOREACH(const std::string& index, vs.constant_indices) {
      stream << "[" << index << "]";
    }
  }
  if (vs.type) {
    unsigned int width = vs._width();
    if (width > 0)
      stream << ", width = " << width;
  }
  stream << ")";
  return stream;
}

std::ostream& operator <<(std::ostream& stream, ValueStorageP vs) {
  if (vs)
    return stream << *vs;
  else
    return stream << "static_cast<ValueStorage*>(NULL)";
}


class ValueStorageFactory {
  std::map<Value*,      ValueStorageP> by_llvm_value;
  std::map<std::string, ValueStorageP> by_name;

  struct ValueAndIndex {
    ValueStorageP value;
    std::string index;

    ValueAndIndex(ValueStorageP value, const std::string& index) : value(value), index(index) { }
    ValueAndIndex(ValueStorageP value) : value(value) { }

    bool operator <(const ValueAndIndex& other) const {
      return value < other.value || value == other.value && index < other.index;
    }
  };
  std::map<ValueAndIndex, ValueStorageP> by_index;

  UniqueNameSource tmpVarNameGenerator;
public:
  ValueStorageFactory() : tmpVarNameGenerator("t") { }

  void clear() {
    by_llvm_value.clear();
    by_name.clear();
    by_index.clear();
    tmpVarNameGenerator.reset();
  }

  ValueStorageP makeParameter(Value* value, const std::string& name) {
    assert(!contains(by_llvm_value, value));
    assert(!contains(by_name, name));

    ValueStorageP x(new ValueStorage());
    x->kind = ValueStorage::FUNCTION_PARAMETER;
    x->name = name;
    x->type = value->getType();

    by_llvm_value[value] = x;
    by_name[name] = x;

    return x;
  }

  ValueStorageP makeTemporaryVariable(Value* value) {
    assert(!contains(by_llvm_value, value));

    ValueStorageP x(new ValueStorage());
    x->kind = ValueStorage::VARIABLE;
    x->name = tmpVarNameGenerator.next();
    x->type = value->getType();

    by_llvm_value[value] = x;
    by_name[x->name] = x;

    return x;
  }

  ValueStorageP getOrCreateTemporaryVariable(Value* value) {
    if (ValueStorageP* var = getValueOrNull(by_llvm_value, value))
      return *var;
    else
      return makeTemporaryVariable(value);
  }

  ValueStorageP getTemporaryVariable(const std::string& name) {
    if (ValueStorageP* var = getValueOrNull(by_name, name))
      return *var;
    else
      assert(false);
  }

  ValueStorageP getGlobalVariable(Value* v);
  ValueStorageP getGlobalVariable(const std::string& name, Type* type);
  ValueStorageP getAtConstIndex(ValueStorageP ptr, Value* index);
  ValueStorageP getAtConstIndex(ValueStorageP ptr, std::string str_index);
  ValueStorageP getConstant(const std::string& constant, unsigned int width);

  ValueStorageP get(Value* value) {
    ValueStorageP vs = by_llvm_value[value];
    if (!vs)
      vs = get2(value);
    debug_print("ValueStorageFactory::get(" << value << ") -> " << vs);
    assert(vs);
    return vs;
  }

  void set(Value* target, ValueStorageP source) {
    assert(!contains(by_llvm_value, target));

    by_llvm_value[target] = source;
  }

private:
  ValueStorageP get2(Value* value);
};


struct Channel {
  Direction direction;
  std::string constant;

  ::Operator* component;
  std::string data_signal, valid_signal, ready_signal;
  unsigned int width;

  static ChannelP make_constant(const std::string& constant, unsigned int width) {
    ChannelP ch(new Channel());
    ch->direction = CONSTANT_OUT;
    ch->constant  = constant;
    ch->width     = width;
    return ch;
  }
  static ChannelP make_port(const std::string& name, Direction direction, unsigned int width) {
    ChannelP ch(new Channel());
    ch->direction = direction;
    ch->width     = width;
    ch->data_signal  = name + "_data";
    ch->valid_signal = name + "_valid";
    ch->ready_signal = name + "_ready";
    return ch;
  }
  static ChannelP make_input(const std::string& name, unsigned int width) {
    return make_port(name, OUT, width);
  }
  static ChannelP make_output(const std::string& name, unsigned int width) {
    return make_port(name, IN, width);
  }
  static ChannelP make_component_port(
      ::Operator* component, const std::string& name, Direction direction) {
    ChannelP ch(new Channel());
    ch->direction = direction;
    ch->component = component;
    ch->data_signal  = name + "_data";
    ch->valid_signal = name + "_valid";
    ch->ready_signal = name + "_ready";
    ch->width = component->getSignalByName(ch->data_signal)->width();
    return ch;
  }
  static ChannelP make_component_input(::Operator* component, const std::string& name) {
    return make_component_port(component, name, IN);
  }
  static ChannelP make_component_output(::Operator* component, const std::string& name) {
    return make_component_port(component, name, OUT);
  }

  static ChannelP make_temp(::Operator* op, const std::string& name, unsigned int width) {
    return make_variable(op, name, width);
  }

  static ChannelP make_variable(::Operator* op, const std::string& name, unsigned int width) {
    ChannelP ch(new Channel());
    ch->direction = INOUT;
    ch->width     = width;
    ch->data_signal  = op->declare(name + "_data", width, true);
    ch->valid_signal = op->declare(name + "_valid");
    ch->ready_signal = op->declare(name + "_ready");
    return ch;
  }

  void addTo(::Operator* op) {
    std::cout << "addTo: " << data_signal << std::endl;
    if (direction == IN) {
      op->addOutput(data_signal, width);
      op->addOutput(valid_signal);
      op->addInput(ready_signal);
    } else {
      op->addInput(data_signal, width);
      op->addInput(valid_signal);
      op->addOutput(ready_signal);
    }
  }

  void generateSignal(MyOperator* op) {
    if (component && direction != CONSTANT_OUT) {
      //TODO we have to use the instance name instead of the component name
      std::string
        data_signal_new  = component->getName() + "_" + data_signal,
        valid_signal_new = component->getName() + "_" + valid_signal,
        ready_signal_new = component->getName() + "_" + ready_signal;
      if (direction == OUT) {
        op->outPortMap(component, data_signal, data_signal_new);
        op->outPortMap(component, data_signal, data_signal_new);
        op->inPortMap(component, data_signal, data_signal_new);
      } else {
        op->inPortMap(component, data_signal, data_signal_new);
        op->inPortMap(component, data_signal, data_signal_new);
        op->outPortMap(component, data_signal, data_signal_new);
      }

      component = NULL;
      data_signal  = data_signal_new;
      valid_signal = valid_signal_new;
      ready_signal = ready_signal_new;
    }
  }

  void connectToOutput(Channel* ch, MyOperator* op) {
    assert(width == ch->width);

    if (direction & IN) {
      if (ch->direction == CONSTANT_OUT) {
        if (!component) {
          *op << "   " << data_signal << " <= " << ch->constant << ";\n";
          *op << "   " << valid_signal << " <= '1';\n";
          // ignore ready_signal
        } else {
          op->inPortMapCst(component, data_signal, ch->constant);
          op->inPortMapCst(component, valid_signal, "'1'");
          // ignore ready_signal
        }
      } else if (ch->direction & OUT) {
        if (component && ch->component) {
          // both are component ports -> generate a signal to connect them
          // We generate the signal for the output port because it can be reused.
          ch->generateSignal(op);

          assert(!ch->component);
        }

        if (component && !ch->component) {
          op->inPortMap (component, data_signal,  ch->data_signal);
          op->inPortMap (component, valid_signal, ch->valid_signal);
          op->outPortMap(component, ready_signal, ch->ready_signal + "_");
          *op << "   " << ch->ready_signal << " <= " << ch->ready_signal << "_" << ";\n";
        } else if (!component && ch->component) {
          op->outPortMap(ch->component, ch->data_signal,  data_signal + "_");
          *op << "   " << data_signal << " <= " << data_signal << "_" << ";\n";
          op->outPortMap(ch->component, ch->valid_signal, valid_signal + "_");
          *op << "   " << valid_signal << " <= " << valid_signal << "_" << ";\n";
          op->inPortMap (ch->component, ch->ready_signal, ready_signal);
        } else if (!component && !ch->component) {
          *op << "   " << data_signal  << " <= " << ch->data_signal << ";\n";
          *op << "   " << valid_signal << " <= " << ch->valid_signal << ";\n";
          *op << "   " << ch->ready_signal << " <= " << ready_signal << ";\n";
        } else {
          assert(false);
        }
      } else {
        throw std::string("Cannot connect input with input!");
      }
    } else {
      if (ch->direction == IN)
        throw std::string("The argument must be an output!");
      else
        throw std::string("Cannot connect output with output!");
    }
  }

  void connectToOutput(ChannelP ch, MyOperator* op) {
    connectToOutput(ch.get(), op);
  }

  void connectToInput(Channel* ch, MyOperator* op) {
    ch->connectToOutput(this, op);
  }

  void connectToInput(ChannelP ch, MyOperator* op) {
    connectToInput(ch.get(), op);
  }
};

ChannelP ValueStorage::getReadChannel(MyOperator* op) {
  if (!channel_read) {
    switch (kind) {
      case FUNCTION_PARAMETER:
      case GLOBAL_VARIABLE:
        channel_read = Channel::make_input(name + "_in", width());
        channel_read->addTo(op);
        break;
      case VARIABLE:
        channel_read = Channel::make_variable(op, name, width());
        channel_write = channel_read;
        break;
      default:
        assert(false);
        break;
    }
  }

  assert(channel_read && ChannelDirection::matching_direction(channel_read->direction, OUT));

  return channel_read;
}

ChannelP ValueStorage::getWriteChannel(MyOperator* op) {
  if (!channel_write) {
    switch (kind) {
      case FUNCTION_PARAMETER:
      case GLOBAL_VARIABLE:
        channel_write = Channel::make_output(name + "_out", width());
        channel_write->addTo(op);
        break;
      case VARIABLE:
        channel_read = Channel::make_variable(op, name, width());
        channel_write = channel_read;
        break;
      default:
        assert(false);
        break;
    }
  }

  assert(channel_write && ChannelDirection::matching_direction(channel_write->direction, IN));

  return channel_write;
}

std::string VHDLBackend::generateBranchLabel(Value *target) {
  std::string label = branchLabelNameGenerator.next();
  branchLabels[target].push_back(label);
  debug_print("generateBranchLabel(" << target << ") -> " << label);
  return label;
}


ValueStorageP ValueStorageFactory::getGlobalVariable(Value* value) {
  std::string name = value->getName().data();
  if (!contains(by_name, name))
    assert(!contains(by_llvm_value, value));
  ValueStorageP x = getGlobalVariable(name, value->getType());
  by_llvm_value[value] = x;
  return x;
}

ValueStorageP ValueStorageFactory::getGlobalVariable(const std::string& name, Type* type) {
  ValueStorageP x = by_name[name];
  if (!x) {
    x = ValueStorageP(new ValueStorage());
    x->kind = ValueStorage::GLOBAL_VARIABLE;
    x->name = name;
    x->type = type;

    by_name[name] = x;
  }
  return x;
}

ValueStorageP ValueStorageFactory::getAtConstIndex(ValueStorageP ptr, Value* index) {
  //NOTE: This will only work for constant ints.
  std::string str_index = dyn_cast<ConstantInt>(index)->getValue().toString(10, true);

  return getAtConstIndex(ptr, str_index);
}

ValueStorageP ValueStorageFactory::getAtConstIndex(ValueStorageP ptr, std::string str_index) {
  ValueStorageP pointee = by_index[ValueAndIndex(ptr, str_index)];

  if (!pointee) {
    pointee = ValueStorageP(new ValueStorage());
    pointee->kind = ptr->kind;
    pointee->name = ptr->name + "_" + str_index;
    assert(ptr->type);
    pointee->type = ptr->type->getSequentialElementType();

    by_index[ValueAndIndex(ptr, str_index)] = pointee;
  }

  return pointee;
}


void ValueStorage::initWithChannels(ChannelP channel_read, ChannelP channel_write) {
  assert(!channel_read  || ChannelDirection::matching_direction(channel_read ->direction, OUT));
  assert(!channel_write || ChannelDirection::matching_direction(channel_write->direction, IN ));

  this->kind = CHANNEL;
  this->channel_read  = channel_read;
  this->channel_write = channel_write;
}

ValueStorageP ValueStorageFactory::getConstant(const std::string& constant, unsigned int width) {
  ValueStorageP x(new ValueStorage());
  ChannelP read_channel = Channel::make_constant(constant, width);
  x->initWithChannels(read_channel, ChannelP((Channel*)NULL));
  return x;
}

ValueStorageP ValueStorageFactory::get2(Value* value) {
  // handle global value operands
  if (llvm::GEPOperator *op = dyn_cast<GEPOperator>(value)) {
    if (!op->hasAllConstantIndices()) {
      errs() << "ERROR: Variable indices are not supported!\n";
      assert(false);
    }

    ValueStorageP x = getGlobalVariable(op->getPointerOperand());

    if (op->hasIndices()) {
      // NOTE: the last index is the one we want to know
      Value* index;
      for (User::op_iterator it = op->idx_begin(); it != op->idx_end(); ++it)
        index = *it;
      x = getAtConstIndex(x, index);
    }
    
    return x;
  }

  // handle constant integers
  else if (ConstantInt *ci = dyn_cast<ConstantInt>(value))
    return getConstant(ci->getValue().toString(10, true), value->getType()->getPrimitiveSizeInBits());

  // handle constant floating point numbers
  else if (ConstantFP *cf = dyn_cast<ConstantFP>(value)) {
    double dvalue = cf->getValueAPF().convertToDouble();
    return getConstant(toString(dvalue), value->getType()->getPrimitiveSizeInBits());
  } else {
    assert(false);
  }
}


VHDLBackend::VHDLBackend()
  : branchLabelNameGenerator("label"),
    instanceNameGenerator("inst"),
    vs_factory(new ValueStorageFactory()) { }

void VHDLBackend::init(SimpleCCodeGenerator* generator, std::ostream& stream) {
  this->generator = generator;
  this->stream = &stream;

  branchLabels.clear();
  branchLabelNameGenerator.reset();
  vs_factory->clear();

  op.reset(new MyOperator());
  op->setName("test");
  op->setCopyrightString("blub");
  op->addPort("Clk",1,1,1,0,0,0,0,0,0,0,0);
}

void VHDLBackend::generateStore(Value *op1, Value *op2) {
  debug_print("generateStore(" << op1 << ", " << op2 << ")");
  return_if_dry_run();

  ChannelP ch1 = vs_factory->get(op1)->getWriteChannel(op.get());
  ChannelP ch2 = vs_factory->get(op2)->getReadChannel(op.get());

  ch1->connectToOutput(ch2, op.get());

  vs_factory->get(op1)->replaceBy(ch2);
}

struct OperatorInfo {
  ::Operator* op;
  std::string input1, input2, output;
};

OperatorInfo getBinaryOperator(unsigned opcode, unsigned width) {
  //TODO use width
  std::string name;
  switch (opcode) {
  case Instruction::FAdd:
    name = "fpadd";
    break;
  case Instruction::Add:
    name = "add";
    break;
  case Instruction::FSub:
    name = "fpsub";
    break;
  case Instruction::Sub:
    name = "sub";
    break;
  case Instruction::FMul:
    name = "fpmul";
    break;
  case Instruction::Mul:
    name = "mul";
    break;
  case Instruction::FDiv:
    name = "fpdiv";
    break;
  case Instruction::UDiv:
    name = "udiv";
    break;
  case Instruction::SDiv:
    name = "sdiv";
    break;
  case Instruction::URem:
    name = "urem";
    break;
  case Instruction::SRem:
    name = "srem";
    break;
  case Instruction::FRem:
    name = "frem";
    break;
  case Instruction::Or:
    name = "or_";
    break;
  case Instruction::And:
    name = "and_";
    break;
  default:
    assert(false);
  }

  //TODO load from PivPav
  ::Operator* op = new ::Operator();
  op->setName(name);
  op->addPort  ("Clk",1,1,1,0,0,0,0,0,0,0,0);
  op->addInput ("a_data", width);
  op->addInput ("b_data", width);
  op->addOutput("result_data", width);
  op->addInput ("a_valid");
  op->addInput ("b_valid");
  op->addOutput("result_valid");
  op->addOutput("a_ready");
  op->addOutput("b_ready");
  op->addInput ("result_ready");

  OperatorInfo op_info = { op, "a", "b", "result" };
  return op_info;
}

void VHDLBackend::generateBinaryOperator(std::string tmpVar,
    Value *op1, Value *op2, unsigned opcode) {
  debug_print("generateBinaryOperator(" << tmpVar << ", " << op1 << ", " << op2 << ", "
    << Instruction::getOpcodeName(opcode) << ")");
  return_if_dry_run();

  ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);

  OperatorInfo op_info = getBinaryOperator(opcode, tmp->width());

  ChannelP input1 = Channel::make_component_input (op_info.op, op_info.input1);
  ChannelP input2 = Channel::make_component_input (op_info.op, op_info.input2);
  ChannelP output = Channel::make_component_output(op_info.op, op_info.output);

  input1->connectToOutput(vs_factory->get(op1)->getReadChannel(op.get()), op.get());
  input2->connectToOutput(vs_factory->get(op2)->getReadChannel(op.get()), op.get());
  output->connectToInput (tmp->getWriteChannel(op.get()), op.get());

  this->op->inPortMap(op_info.op, "Clk", "Clk");

  *this->op << this->op->instance(op_info.op, tmpVar);
}

Type* getElementType(Type* type) {
  while (isa<SequentialType>(type))
    type = type->getSequentialElementType();

  return type;
}

unsigned int getWidthOfElementType(Type* type) {
  int w = getElementType(type)->getPrimitiveSizeInBits();
  assert(w != 0);
  return w;
}

void VHDLBackend::generateCall(std::string funcName,
    std::string tmpVar, std::vector<Value*> args) {
  debug_print("generateCall(" << funcName << ", " << tmpVar << ", args)");
  return_if_dry_run();

  // create an operator for this function
  ::Operator* func = new ::Operator();
  func->setName(funcName);
  func->addPort  ("Clk",1,1,1,0,0,0,0,0,0,0,0);

  std::vector<std::string> argnames;
  int i = 0;
  BOOST_FOREACH(Value* arg, args) {
    std::stringstream argname_stream;
    argname_stream << "arg" << (i++);
    std::string argname = argname_stream.str();
    argnames.push_back(argname);

    func->addInput (argname + "_data", getWidthOfElementType(arg->getType()));
    func->addInput (argname + "_valid");
    func->addOutput(argname + "_ready");
  }
  if (!tmpVar.empty()) {
    ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);
    func->addOutput("result_data", getWidthOfElementType(tmp->type));
    func->addOutput("result_valid");
    func->addInput ("result_ready");
  } else {
    func->addOutput("done");
  }

  this->op->inPortMap(func, "Clk", "Clk");

  i = 0;
  BOOST_FOREACH(Value* arg, args) {
    const std::string& argname = argnames[i++];
    ChannelP argchannel = Channel::make_component_input(func, argname);
    argchannel->connectToOutput(vs_factory->get(arg)->getReadChannel(op.get()), op.get());
  }

  if (!tmpVar.empty()) {
    ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);
    ChannelP result = Channel::make_component_output(func, "result");
    result->connectToInput(tmp->getWriteChannel(op.get()), op.get());
  } else {
    //TODO: make sure that we wait for the done signal to become '1'
  }

  std::string instance_name = (!tmpVar.empty() ? tmpVar : instanceNameGenerator.next());
  *this->op << this->op->instance(func, instance_name);
}

void VHDLBackend::generateVoidCall(std::string funcName, std::vector<Value*> args) {
  debug_print("generateVoidCall(" << funcName << ", args)");
  return_if_dry_run();
  
  generateCall(funcName, "", args);
}

void VHDLBackend::generateComparison(std::string tmpVar, Value *op1, Value *op2,
    FCmpInst::Predicate comparePredicate) {
  debug_print("generateComparison(" << tmpVar << ", " << op1 << ", " << op2 << ")");
  return_if_dry_run();
  /*stream << "\t" << tmpVar
    <<  " = ("
    <<  getOperandString(op1)
    <<  " " << ">" /*CCodeMaps::parseComparePredicate(comparePredicate)* / << " "
    <<  getOperandString(op2)
    <<  ");\n";*/
}

void VHDLBackend::generateIntegerExtension(std::string tmpVar, Value *op) {
  debug_print("generateComparison(" << tmpVar << ", " << op << ")");
  return_if_dry_run();
  /*stream << "\t" << tmpVar
    <<  " = "
    <<  "(int)"
    <<  getOperandString(op)
    <<  ";\n";*/
}

void VHDLBackend::generatePhiNodeAssignment(std::string tmpVar, Value *op) {
  debug_print("generatePhiNodeAssignment(" << tmpVar << ", " << op << ")");
  return_if_dry_run();
  /*stream << "\t" << tmpVar
    <<  " = "
    <<  getOperandString(op)
    <<  ";\n";*/
}

void VHDLBackend::generateUnconditionalBranch(std::string label) {
  debug_print("generateUnconditionalBranch(" << label << ")");
  return_if_dry_run();
  //stream << "\tgoto " << label << ";\n";
}

void VHDLBackend::generateConditionalBranch(Value *condition,
    std::string label1, std::string label2) {
  debug_print("generateConditionalBranch(" << condition << ", " << label1 << ", " << label2 << ")");
  return_if_dry_run();
  /*stream << "\tif (" << getOperandString(condition) << ")"
    <<  " goto " << label1 << ";"
    <<  " else goto " << label2
    << ";\n";*/
}

void VHDLBackend::generateReturn(Value *retVal) {
  debug_print("generateReturn(" << retVal << ")");
  return_if_dry_run();
  //stream << "\treturn " << getOperandString(retVal) << ";\n";
}


void VHDLBackend::generateVariableDeclarations(const std::map<std::string, std::vector<std::string> >& tmpVariables) {
  debug_print("generateVariableDeclarations(tmpVariables)");
  return_if_dry_run();
  /*for (std::map<std::string, std::vector<std::string> >::const_iterator it=tmpVariables.begin();
      it!=tmpVariables.end(); ++it) {
    stream << "\t" << it->first << " ";

    Seperator sep(", ");
    for (std::vector<std::string>::const_iterator varIt = it->second.begin(); varIt != it->second.end(); ++varIt)
      stream << sep << *varIt;

    stream << ";\n";
  }*/
}

void VHDLBackend::generateBranchTargetIfNecessary(llvm::Instruction* instr) {
  debug_print("generateBranchTargetIfNecessary(" << instr << ")");
  return_if_dry_run();
  /*llvm::Value* instr_as_value = instr;
  if (std::vector<std::string>* labels = getValueOrNull(branchLabels, instr_as_value)) {
    for (std::vector<std::string>::iterator it = labels->begin(); it != labels->end(); ++it)
      stream << *it << ":\n";
  }*/
}

void VHDLBackend::generateEndOfMethod() {
  debug_print("generateEndOfMethod()");
  return_if_dry_run();
  op->outputVHDL(*stream);
}

std::string VHDLBackend::createTemporaryVariable(Value *addr) {
  debug_print("createTemporaryVariable(" << addr << ")");
  return_value_if_dry_run(tmpVarNameGenerator.next());

  ValueStorageP x = vs_factory->makeTemporaryVariable(addr);
  debug_print(" -> " << x->name);
  return x->name;
}

std::string VHDLBackend::getOrCreateTemporaryVariable(Value *addr) {
  ValueStorageP x = vs_factory->getOrCreateTemporaryVariable(addr);
  debug_print(" -> " << x->name);
  return x->name;
}

void VHDLBackend::addVariable(Value *addr, std::string name) {
  debug_print("addVariable(" << addr << ", " << name << ")");
  return_if_dry_run();

  ValueStorageP variable = vs_factory->getGlobalVariable(name, addr->getType());
  vs_factory->set(addr, variable);
}

void VHDLBackend::addParameter(Value *addr, std::string name) {
  debug_print("addParameter(" << addr << ", " << name << ")");
  vs_factory->makeParameter(addr, name);
}

void VHDLBackend::addVariable(Value *addr, std::string name, std::string index) {
  debug_print("addVariable(" << addr << ", " << name << ", " << index << ")");
  return_if_dry_run();

  if (index.empty()) {
    addVariable(addr, name);
    return;
  }

  // We load the value via an index, so the type of the variable must
  // be a pointer to the resulting type (which is the type of addr).
  Type* ptr_type = PointerType::get(addr->getType(), 0);

  ValueStorageP variable = vs_factory->getGlobalVariable(name, ptr_type);
  variable = vs_factory->getAtConstIndex(variable, index);
  vs_factory->set(addr, variable);
}

void VHDLBackend::copyVariable(Value *source, Value *target) {
  debug_print("copyVariable(" << source << ", " << target << ")");
  return_if_dry_run();
  //debug_print("  " << channels[source]->data_signal);
  //channels[target] = channels[source];
  vs_factory->set(target, vs_factory->get(source));
}

void VHDLBackend::addIndexToVariable(Value *source, Value *target, std::string index) {
  debug_print("addVariable(" << source << ", " << target << ", " << index << ")");
  return_if_dry_run();

  ValueStorageP vs_source = vs_factory->get(source);
  ValueStorageP vs_source_with_index = vs_factory->getAtConstIndex(vs_source, index);
  vs_factory->set(target, vs_source_with_index);
}
