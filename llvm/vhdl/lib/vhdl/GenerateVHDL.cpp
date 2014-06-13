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

#include "mehari/utils/ContainerUtils.h"
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

void MyOperator::stdLibs(std::ostream& o) {
  o << "library ieee;\n";
  o << "use ieee.std_logic_1164.all;\n";
  o << "use ieee.std_logic_unsigned.all;\n";
  o << "use ieee.math_real.ALL;\n";
  o << "use ieee.numeric_std.all;\n";
  o << "\n";
  o << "library work;\n";
  o << "use work.float_helpers.all;\n";
  o << "use work.test_helpers.all;\n";
  o << "\n";
}


template<typename T>
inline MyOperator& operator <<(MyOperator& op, const T& x) {
  op.addCode(x);
  return op;
}

struct OperatorInfo {
  ::Operator* op;
  std::string input1, input2, output;
  std::string data_suffix, valid_suffix, ready_suffix;
  std::string code;
};

typedef boost::shared_ptr<class Channel> ChannelP;

#include "ValueStorage.incl.cpp"
#include "ReadySignals.incl.cpp"
#include "Channel.incl.cpp"

ChannelP ValueStorage::getReadChannel(MyOperator* op) {
  if (!channel_read) {
    switch (kind) {
      case FUNCTION_PARAMETER:
      case GLOBAL_VARIABLE:
        channel_read = Channel::make_input(name + "_in", width());
        channel_read->addTo(op);
        break;
      case TEMPORARY_VARIABLE:
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
      case TEMPORARY_VARIABLE:
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
  else if (ConstantInt *ci = dyn_cast<ConstantInt>(value)) {
    std::string constant = ci->getValue().toString(10, true);
    unsigned int width = value->getType()->getPrimitiveSizeInBits();
    constant = "std_logic_vector(to_unsigned(" + constant + ", " + width + "))";
    return getConstant(constant, width);
  }

  // handle constant floating point numbers
  else if (ConstantFP *cf = dyn_cast<ConstantFP>(value)) {
    double dvalue = cf->getValueAPF().convertToDouble();
    
    //std::string constant = toString(dvalue);
    // toString generates human-friendly output, e.g. "1" for 1.0,
    // but in VHDL this is an integer not a real value.
    char buf[64];
    snprintf(buf, sizeof(buf), "%f", dvalue);
    std::string constant(buf);

    unsigned int width = value->getType()->getPrimitiveSizeInBits();
    constant = "to_float(" + constant + ")";
    return getConstant(constant, width);
  } else {
    assert(false);
  }
}


VHDLBackend::VHDLBackend()
  : instanceNameGenerator("inst"),
    vs_factory(new ValueStorageFactory()),
    ready_signals(new ReadySignals()) { }

MyOperator* VHDLBackend::getOperator() {
  return op.get();
}

void VHDLBackend::init(SimpleCCodeGenerator* generator, std::ostream& stream) {
  this->generator = generator;
  this->stream = &stream;

  vs_factory->clear();
  instanceNameGenerator.reset();
  usedVariableNames.reset();
  ready_signals->clear();

  op.reset(new MyOperator());
  op->setName("test");
  op->setCopyrightString("blub");
  op->addPort("aclk",1,1,1,0,0,0,0,0,0,0,0);
  op->addPort("reset",1,1,0,1,0,0,0,0,0,0,0);
}

void VHDLBackend::generateStore(Value *op1, Value *op2) {
  debug_print("generateStore(" << op1 << ", " << op2 << ")");
  return_if_dry_run();

  ChannelP ch1 = vs_factory->getForWriting(op1)->getWriteChannel(op.get());
  ChannelP ch2 = vs_factory->get(op2)->getReadChannel(op.get());

  ch1->connectToOutput(ch2, op.get(), usedVariableNames, *ready_signals);

  vs_factory->get(op1)->replaceBy(ch2);
}

OperatorInfo getBinaryOperator(unsigned opcode, unsigned width) {
  //TODO use width
  std::string name;
  bool is_floating_point = false;
  switch (opcode) {
  case Instruction::FAdd:
    name = "float_add";
    is_floating_point = true;
    break;
  case Instruction::Add:
    name = "add";
    break;
  case Instruction::FSub:
    name = "fpsub";
    is_floating_point = true;
    break;
  case Instruction::Sub:
    name = "sub";
    break;
  case Instruction::FMul:
    name = "fpmul";
    is_floating_point = true;
    break;
  case Instruction::Mul:
    name = "mul";
    break;
  case Instruction::FDiv:
    name = "fpdiv";
    is_floating_point = true;
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
    is_floating_point = true;
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

  std::string input_prefix, output_prefix, data_suffix, valid_suffix, ready_suffix;
  if (is_floating_point) {
    input_prefix  = "s_axis_";
    output_prefix = "m_axis_";
    data_suffix   = "_tdata";
    valid_suffix  = "_tvalid";
    ready_suffix  = "_tready";
  } else {
    data_suffix  = "_data";
    valid_suffix = "_valid";
    ready_suffix = "_ready";
  }

  //TODO load from PivPav
  ::Operator* op = new ::Operator();
  op->setName(name);
  op->addPort  ("aclk",1,1,1,0,0,0,0,0,0,0,0);
  op->addInput (input_prefix  + "a"      + data_suffix, width);
  op->addInput (input_prefix  + "b"      + data_suffix, width);
  op->addOutput(output_prefix + "result" + data_suffix, width);
  op->addInput (input_prefix  + "a"      + valid_suffix);
  op->addInput (input_prefix  + "b"      + valid_suffix);
  op->addOutput(output_prefix + "result" + valid_suffix);
  op->addOutput(input_prefix  + "a"      + ready_suffix);
  op->addOutput(input_prefix  + "b"      + ready_suffix);
  op->addInput (output_prefix + "result" + ready_suffix);

  OperatorInfo op_info = { op,
    input_prefix+"a", input_prefix+"b", output_prefix+"result",
    data_suffix, valid_suffix, ready_suffix
  };
  return op_info;
}

void VHDLBackend::generateBinaryOperator(std::string tmpVar,
    Value *op1, Value *op2, unsigned opcode) {
  debug_print("generateBinaryOperator(" << tmpVar << ", " << op1 << ", " << op2 << ", "
    << Instruction::getOpcodeName(opcode) << ")");
  return_if_dry_run();

  ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);

  OperatorInfo op_info = getBinaryOperator(opcode, tmp->width());

  ChannelP input1 = Channel::make_component_input (op_info.op, op_info.input1, op_info);
  ChannelP input2 = Channel::make_component_input (op_info.op, op_info.input2, op_info);
  ChannelP output = Channel::make_component_output(op_info.op, op_info.output, op_info);

  input1->connectToOutput(vs_factory->get(op1)->getReadChannel(op.get()), op.get(), usedVariableNames, *ready_signals);
  input2->connectToOutput(vs_factory->get(op2)->getReadChannel(op.get()), op.get(), usedVariableNames, *ready_signals);
  output->connectToInput (tmp->getWriteChannel(op.get()),                 op.get(), usedVariableNames, *ready_signals);

  this->op->inPortMap(op_info.op, "aclk", "aclk");

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
  func->addPort  ("aclk",1,1,1,0,0,0,0,0,0,0,0);

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

  this->op->inPortMap(func, "aclk", "aclk");

  i = 0;
  BOOST_FOREACH(Value* arg, args) {
    const std::string& argname = argnames[i++];
    ChannelP argchannel = Channel::make_component_input(func, argname);
    argchannel->connectToOutput(vs_factory->get(arg)->getReadChannel(op.get()), op.get(), usedVariableNames, *ready_signals);
  }

  if (!tmpVar.empty()) {
    ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);
    ChannelP result = Channel::make_component_output(func, "result");
    result->connectToInput(tmp->getWriteChannel(op.get()), op.get(), usedVariableNames, *ready_signals);
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

OperatorInfo getComparisonOperator(FCmpInst::Predicate comparePredicate, unsigned width) {
  std::string name;
  bool is_floating_point = false;
  std::string code;
  bool negate = false;
  switch (comparePredicate) {
    case FCmpInst::FCMP_FALSE:
      "false";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_OEQ:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00010100'";
      break;
    case FCmpInst::FCMP_OGT:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00100100'";
      break;
    case FCmpInst::FCMP_OGE:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00110100'";
      break;
    case FCmpInst::FCMP_OLT:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00001100'";
      break;
    case FCmpInst::FCMP_OLE:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00011100'";
      break;
    case FCmpInst::FCMP_ONE:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00101100'";
      break;
    case FCmpInst::FCMP_ORD:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00000100'";
      negate = true;  // not supported
      assert(false);
      break;
    case FCmpInst::FCMP_UNO:
      name = "float_cmp";
      is_floating_point = true;
      code = "'00000100'";
      break;
    case FCmpInst::FCMP_UEQ:
      "isnan || ==";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_UGT:
      "isnan || >";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_UGE:
      "isnan || >=";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_ULT:
      "isnan || <";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_ULE:
      "isnan || <=";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_UNE:
      "isnan || !=";
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_TRUE:
      "true";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_EQ:
      "==";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_NE:
      name = "icmp_ne";
      break;
    case FCmpInst::ICMP_UGT:
      ">";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_UGE:
      ">=";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_ULT:
      "<";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_ULE:
      "<=";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SGT:
      ">";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SGE:
      ">=";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SLT:
      "<";
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SLE:
      "<=";
      assert(false);  // not supported
      break;
    default:
      assert(false);
      break;
  }

  std::string input_prefix, output_prefix, data_suffix, valid_suffix, ready_suffix;
  if (is_floating_point) {
    input_prefix  = "s_axis_";
    output_prefix = "m_axis_";
    data_suffix   = "_tdata";
    valid_suffix  = "_tvalid";
    ready_suffix  = "_tready";
  } else {
    data_suffix  = "_data";
    valid_suffix = "_valid";
    ready_suffix = "_ready";
  }

  //TODO load from PivPav
  ::Operator* op = new ::Operator();
  op->setName(name);
  op->addPort  ("aclk",1,1,1,0,0,0,0,0,0,0,0);
  op->addInput (input_prefix  + "a"      + data_suffix, width);
  op->addInput (input_prefix  + "b"      + data_suffix, width);
  op->addOutput(output_prefix + "result" + data_suffix, 1);
  op->addInput (input_prefix  + "a"      + valid_suffix);
  op->addInput (input_prefix  + "b"      + valid_suffix);
  op->addOutput(output_prefix + "result" + valid_suffix);
  op->addOutput(input_prefix  + "a"      + ready_suffix);
  op->addOutput(input_prefix  + "b"      + ready_suffix);
  op->addInput (output_prefix + "result" + ready_suffix);

  if (!code.empty()) {
    op->addInput (input_prefix  + "operation" + data_suffix, 8);
    op->addInput (input_prefix  + "operation" + valid_suffix);
    op->addOutput(input_prefix  + "operation" + ready_suffix);
  }

  OperatorInfo op_info = { op,
    input_prefix+"a", input_prefix+"b", output_prefix+"result",
    data_suffix, valid_suffix, ready_suffix,
    code
  };
  return op_info;
}

void VHDLBackend::generateComparison(std::string tmpVar, Value *op1, Value *op2,
    FCmpInst::Predicate comparePredicate) {
  debug_print("generateComparison(" << tmpVar << ", " << op1 << ", " << op2 << ")");
  return_if_dry_run();

  ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);

  OperatorInfo op_info = getComparisonOperator(comparePredicate, vs_factory->get(op1)->width());

  ChannelP input1 = Channel::make_component_input (op_info.op, op_info.input1, op_info);
  ChannelP input2 = Channel::make_component_input (op_info.op, op_info.input2, op_info);
  ChannelP output = Channel::make_component_output(op_info.op, op_info.output, op_info);

  input1->connectToOutput(vs_factory->get(op1)->getReadChannel(op.get()), op.get(), usedVariableNames, *ready_signals);
  input2->connectToOutput(vs_factory->get(op2)->getReadChannel(op.get()), op.get(), usedVariableNames, *ready_signals);
  output->connectToInput (tmp->getWriteChannel(op.get()),                 op.get(), usedVariableNames, *ready_signals);

  this->op->inPortMap(op_info.op, "aclk", "aclk");

  if (!op_info.code.empty()) {
    this->op->inPortMapCst(op_info.op, "s_axis_operation_tdata", op_info.code);
    this->op->inPortMapCst(op_info.op, "s_axis_operation_tvalid", "'1'");
  }

  *this->op << this->op->instance(op_info.op, tmpVar);
}

void VHDLBackend::generateIntegerExtension(std::string tmpVar, Value *op) {
  debug_print("generateComparison(" << tmpVar << ", " << op << ")");
  return_if_dry_run();
  /*stream << "\t" << tmpVar
    <<  " = "
    <<  "(int)"
    <<  getOperandString(op)
    <<  ";\n";*/
  //TODO
}

void VHDLBackend::generatePhiNodeAssignment(std::string tmpVar, Value *op) {
  debug_print("generatePhiNodeAssignment(" << tmpVar << ", " << op << ")");
  return_if_dry_run();
  /*stream << "\t" << tmpVar
    <<  " = "
    <<  getOperandString(op)
    <<  ";\n";*/
  //TODO
}

void VHDLBackend::generateUnconditionalBranch(Instruction *target) {
  debug_print("generateUnconditionalBranch(" << target << ")");
  return_if_dry_run();
  
  vs_factory->makeUnconditionalBranch(target);
}

void VHDLBackend::generateConditionalBranch(Value *condition, Instruction *targetTrue, Instruction *targetFalse) {
  debug_print("generateConditionalBranch(" << condition << ", " << targetTrue << ", " << targetFalse << ")");
  return_if_dry_run();

  vs_factory->makeConditionalBranch(condition, targetTrue, targetFalse);
}

void VHDLBackend::generateBranchTargetIfNecessary(llvm::Instruction* instr) {
  debug_print("generateBranchTargetIfNecessary(" << instr << ")");
  return_if_dry_run();
  
  vs_factory->beforeInstruction(instr, this);
}

ValueStorageP VHDLBackend::remember(ValueStorageP value) {
  ValueStorageP remembered = vs_factory->makeAnonymousTemporaryVariable(value->type);
  ChannelP remembered_write = remembered->getWriteChannel(op.get());

  ChannelP read = value->getReadChannel(op.get());

  if (read->direction == ChannelDirection::CONSTANT_OUT) {
    *op << "   " << remembered_write->valid_signal << " <= '1';\n"
        << "   " << remembered_write->data_signal  << " <= " << read->constant << ";\n";
  } else {
    *op << "   remember_" << remembered->name << " : process(aclk)\n"
        << "   begin\n"
        << "      if reset = '1' then\n"
        << "         " << remembered_write->valid_signal << " <= '0';\n"
        << "         " << remembered_write->data_signal  << " <= (others => '0');\n"
        << "      elsif rising_edge(aclk) and " << read->valid_signal << " == '1' then\n"
        << "         " << remembered_write->valid_signal << " <= " << read->valid_signal << ";\n"
        << "         " << remembered_write->data_signal  << " <= " << read->data_signal  << ";\n"
        << "      end if;\n"
        << "   end process;\n";

    ready_signals->addConsumer(read->ready_signal, "'1'");
  }

  return remembered;
}

void VHDLBackend::generatePhiNode(ValueStorageP target, Value* condition, ValueStorageP trueValue, ValueStorageP falseValue) {
  // make sure we cannot miss the valid signal
  ChannelP condition_rem  = remember(vs_factory->get(condition))->getReadChannel(op.get());
  ChannelP trueValue_rem  = remember(trueValue)->getReadChannel(op.get());
  ChannelP falseValue_rem = remember(falseValue)->getReadChannel(op.get());

  ChannelP target_w = target->getWriteChannel(op.get());

  *op << "   " << target_w->valid_signal << " <= " << trueValue_rem->valid_signal
          << " WHEN " << condition_rem->valid_signal << " == '1' and " << condition_rem->data_signal << "(0) = '1' ELSE\n"
      << "      " << falseValue_rem->valid_signal
          << " WHEN " << condition_rem->valid_signal << " == '1' and " << condition_rem->data_signal << "(0) = '0' ELSE\n"
      << "      '0';\n";
  *op << "   " << target_w->data_signal << " <= " << trueValue_rem->data_signal
          << " WHEN " << condition_rem->valid_signal << " == '1' and " << condition_rem->data_signal << "(0) = '1' ELSE\n"
      << "      " << falseValue_rem->valid_signal
          << " WHEN " << condition_rem->valid_signal << " == '1' and " << condition_rem->data_signal << "(0) = '0' ELSE\n"
      << "      (others => 'X');\n";
}


void VHDLBackend::generateReturn(Value *retVal) {
  debug_print("generateReturn(" << retVal << ")");
  return_if_dry_run();
  
  ValueStorageP retVal_vs = vs_factory->get(retVal);
  ChannelP ch1 = Channel::make_output("return", retVal_vs->width());
  ch1->addTo(op.get());
  ChannelP ch2 = retVal_vs->getReadChannel(op.get());

  ch1->connectToOutput(ch2, op.get(), usedVariableNames, *ready_signals);
}


void VHDLBackend::generateEndOfMethod() {
  debug_print("generateEndOfMethod()");
  return_if_dry_run();

  std::stringstream s;
  ready_signals->outputVHDL(s);
  *op << s.str();

  op->outputVHDL(*stream);
}

std::string VHDLBackend::createTemporaryVariable(Value *addr) {
  debug_print("createTemporaryVariable(" << addr << ")");
  return_value_if_dry_run(tmpVarNameGenerator.next());

  ValueStorageP x = vs_factory->makeTemporaryVariable(addr);
  debug_print(" -> " << x->name);

  usedVariableNames.addUsedName(x->name);

  return x->name;
}

std::string VHDLBackend::getOrCreateTemporaryVariable(Value *addr) {
  ValueStorageP x = vs_factory->getOrCreateTemporaryVariable(addr);
  debug_print(" -> " << x->name);

  usedVariableNames.addUsedName(x->name);

  return x->name;
}

void VHDLBackend::addVariable(Value *addr, std::string name) {
  debug_print("addVariable(" << addr << ", " << name << ")");
  return_if_dry_run();

  usedVariableNames.addUsedName(name);

  ValueStorageP variable = vs_factory->getGlobalVariable(name, addr->getType());
  vs_factory->set(addr, variable);
}

void VHDLBackend::addParameter(Value *addr, std::string name) {
  debug_print("addParameter(" << addr << ", " << name << ")");
  return_if_dry_run();

  usedVariableNames.addUsedName(name);

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
  vs_factory->set(target, vs_factory->get(source));
}

void VHDLBackend::addIndexToVariable(Value *source, Value *target, std::string index) {
  debug_print("addVariable(" << source << ", " << target << ", " << index << ")");
  return_if_dry_run();

  ValueStorageP vs_source = vs_factory->get(source);
  ValueStorageP vs_source_with_index = vs_factory->getAtConstIndex(vs_source, index);
  vs_factory->set(target, vs_source_with_index);
}
