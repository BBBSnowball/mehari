#include "mehari/CodeGen/GenerateVHDL.h"
#include "mehari/CodeGen/DebugPrint.h"

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


VHDLBackend::VHDLBackend(const std::string& name)
  : name(name),
    instanceNameGenerator("inst"),
    vs_factory(new ValueStorageFactory()),
    ready_signals(new ReadySignals()) { }

MyOperator* VHDLBackend::getOperator() {
  return op.get();
}

ReconOSOperator* VHDLBackend::getReconOSOperator() {
  return r_op.get();
}

std::string VHDLBackend::getInterfaceCode() {
  return interface_ccode.str();
}

void VHDLBackend::init(SimpleCCodeGenerator* generator, std::ostream& stream) {
  this->generator = generator;
  this->stream = &stream;

  vs_factory->clear();
  instanceNameGenerator.reset();
  usedVariableNames.reset();
  ready_signals->clear();
  interface_ccode.str("");

  op.reset(new MyOperator());
  op->setName(name);
  op->setCopyrightString("blub");
  op->addPort("aclk",1,1,1,0,0,0,0,0,0,0,0);
  op->addPort("reset",1,1,0,1,0,0,0,0,0,0,0);

  r_op.reset(new ReconOSOperator());
  r_op->setName("reconos");
  r_op->setCalculation(op.get());
}

void VHDLBackend::generateStore(Value *op1, Value *op2) {
  debug_print("generateStore(" << op1 << ", " << op2 << ")");
  return_if_dry_run();

  ValueStorageP value1 = vs_factory->getForWriting(op1);
  ChannelP ch1 = value1->getWriteChannel(op.get());
  ChannelP ch2 = read(vs_factory->get(op2));

  ch1->connectToOutput(ch2, op.get(), usedVariableNames, *ready_signals);

  vs_factory->get(op1)->replaceBy(ch2);


  if (value1->kind == ValueStorage::GLOBAL_VARIABLE || value1->kind == ValueStorage::FUNCTION_PARAMETER) {
    mboxPut(1, ch1, value1);
  }
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
    name = "bit_or";
    break;
  case Instruction::And:
    name = "bit_and";
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
  op->addInput (input_prefix  + "a"      + data_suffix, width, true);
  op->addInput (input_prefix  + "b"      + data_suffix, width, true);
  op->addOutput(output_prefix + "result" + data_suffix, width, 1, true);
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

  input1->connectToOutput(read(vs_factory->get(op1)),     op.get(), usedVariableNames, *ready_signals);
  input2->connectToOutput(read(vs_factory->get(op2)),     op.get(), usedVariableNames, *ready_signals);
  output->connectToInput (tmp->getWriteChannel(op.get()), op.get(), usedVariableNames, *ready_signals);

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

void VHDLBackend::addDataDependency(Value *valueFromOtherThread, const std::string& isSavedHere) {
  debug_print("addDataDependency(" << valueFromOtherThread << ", " << isSavedHere << ")");
  return_if_dry_run();

  vs_factory->set(valueFromOtherThread, vs_factory->getTemporaryVariable(isSavedHere));
}

void VHDLBackend::generateCall(std::string funcName,
    std::string tmpVar, std::vector<Value*> args) {
  debug_print("generateCall(" << funcName << ", " << tmpVar << ", args)");
  return_if_dry_run();

  if (funcName == "_get_real" || funcName == "_get_int" || funcName == "_get_bool") {
    assert(!tmpVar.empty());
    assert(args.size() == 1);
    assert(isa<llvm::ConstantInt>(args[0]));

    ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);

    const llvm::APInt& mbox_apint = cast<llvm::ConstantInt>(args[0])->getValue();
    assert(mbox_apint.isIntN(32));
    unsigned int mbox = 2 + (unsigned int)mbox_apint.getZExtValue();

    ChannelP mbox_channel = Channel::make_input(tmp->name + "_mbox", tmp->width());
    mbox_channel->addTo(op.get());

    mboxGetWithoutInterface(mbox, mbox_channel);


    ChannelP ch1 = tmp->getWriteChannel(op.get());
    ch1->connectToOutput(mbox_channel, op.get(), usedVariableNames, *ready_signals);

    return;
  } else if (funcName == "_put_real" || funcName == "_put_int" || funcName == "_put_bool") {
    assert(tmpVar.empty());
    assert(args.size() == 2);
    assert(isa<llvm::ConstantInt>(args[1]));

    ValueStorageP value = vs_factory->get(args[0]);
    ChannelP value_read = read(value);

    const llvm::APInt& mbox_apint = cast<llvm::ConstantInt>(args[1])->getValue();
    assert(mbox_apint.isIntN(32));
    unsigned int mbox = 2 + (unsigned int)mbox_apint.getZExtValue();

    ChannelP mbox_channel = Channel::make_output(value->name + "_mbox", value->width());
    mbox_channel->addTo(op.get());

    mboxPutWithoutInterface(mbox, mbox_channel);

    mbox_channel->connectToOutput(value_read, op.get(), usedVariableNames, *ready_signals);

    return;
  } else if (funcName == "_sem_wait") {
    assert(tmpVar.empty());
    assert(args.size() == 1);
    assert(isa<llvm::ConstantInt>(args[0]));

    const llvm::APInt& sem_apint = cast<llvm::ConstantInt>(args[0])->getValue();
    assert(sem_apint.isIntN(32));
    unsigned int sem = 2 + (unsigned int)sem_apint.getZExtValue();

    r_op->semWait("sem" + toString(sem) + "_wait", sem);

    return;
  } else if (funcName == "_sem_post") {
    assert(tmpVar.empty());
    assert(args.size() == 1);
    assert(isa<llvm::ConstantInt>(args[0]));

    const llvm::APInt& sem_apint = cast<llvm::ConstantInt>(args[0])->getValue();
    assert(sem_apint.isIntN(32));
    unsigned int sem = 2 + (unsigned int)sem_apint.getZExtValue();

    r_op->semPost("sem" + toString(sem) + "_post", sem);

    return;
  }

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

    func->addInput (argname + "_data", getWidthOfElementType(arg->getType()), true);
    func->addInput (argname + "_valid");
    func->addOutput(argname + "_ready");
  }
  if (!tmpVar.empty()) {
    ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);
    func->addOutput("result_data", getWidthOfElementType(tmp->type), 1, true);
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
    argchannel->connectToOutput(read(vs_factory->get(arg)), op.get(), usedVariableNames, *ready_signals);
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
# define ignore(x)
  switch (comparePredicate) {
    case FCmpInst::FCMP_FALSE:
      ignore("false");
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
      ignore("isnan || ==");
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_UGT:
      ignore("isnan || >");
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_UGE:
      ignore("isnan || >=");
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_ULT:
      ignore("isnan || <");
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_ULE:
      ignore("isnan || <=");
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_UNE:
      ignore("isnan || !=");
      assert(false);  // not supported
      break;
    case FCmpInst::FCMP_TRUE:
      ignore("true");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_EQ:
      ignore("==");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_NE:
      name = "icmp_ne";
      break;
    case FCmpInst::ICMP_UGT:
      ignore(">");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_UGE:
      ignore(">=");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_ULT:
      ignore("<");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_ULE:
      ignore("<=");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SGT:
      name = "icmp_sgt";
      break;
    case FCmpInst::ICMP_SGE:
      ignore(">=");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SLT:
      ignore("<");
      assert(false);  // not supported
      break;
    case FCmpInst::ICMP_SLE:
      ignore("<=");
      assert(false);  // not supported
      break;
    default:
      assert(false);
      break;
  }
# undef ignore

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
  op->addInput (input_prefix  + "a"      + data_suffix, width, true);
  op->addInput (input_prefix  + "b"      + data_suffix, width, true);
  op->addOutput(output_prefix + "result" + data_suffix, 1, 1, true);
  op->addInput (input_prefix  + "a"      + valid_suffix);
  op->addInput (input_prefix  + "b"      + valid_suffix);
  op->addOutput(output_prefix + "result" + valid_suffix);
  op->addOutput(input_prefix  + "a"      + ready_suffix);
  op->addOutput(input_prefix  + "b"      + ready_suffix);
  op->addInput (output_prefix + "result" + ready_suffix);

  if (!code.empty()) {
    op->addInput (input_prefix  + "operation" + data_suffix, 8, true);
    op->addInput (input_prefix  + "operation" + valid_suffix);
    op->addOutput(input_prefix  + "operation" + ready_suffix);
  }

  if (negate) {
    //TODO
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

  input1->connectToOutput(read(vs_factory->get(op1)),     op.get(), usedVariableNames, *ready_signals);
  input2->connectToOutput(read(vs_factory->get(op2)),     op.get(), usedVariableNames, *ready_signals);
  output->connectToInput (tmp->getWriteChannel(op.get()), op.get(), usedVariableNames, *ready_signals);

  this->op->inPortMap(op_info.op, "aclk", "aclk");

  if (!op_info.code.empty()) {
    this->op->inPortMapCst(op_info.op, "s_axis_operation_tdata", op_info.code);
    this->op->inPortMapCst(op_info.op, "s_axis_operation_tvalid", "'1'");
  }

  *this->op << this->op->instance(op_info.op, tmpVar);
}

void VHDLBackend::generateIntegerExtension(std::string tmpVar, Value *value) {
  debug_print("generateComparison(" << tmpVar << ", " << op << ")");
  return_if_dry_run();

  // only unsigned extension is supported
  //TODO assert(isa<IntegerType>(value->getType()) && dyn_cast<IntegerType>(value->getType())->getSignBit() == 0);

  ChannelP write = vs_factory->getTemporaryVariable(tmpVar)->getWriteChannel(op.get());
  ChannelP read  = this->read(vs_factory->get(value));

  unsigned int added_zeros = write->width - read->width;

  *op << "   " << write->data_signal << " <= \"";
  for (unsigned int i=0;i<added_zeros;i++) {
    *op << '0';
  }
  *op << "\" & " << read->data_signal << ";\n";

  *op << "   " << write->valid_signal << " <= " << read->valid_signal << ";\n";
  ready_signals->addConsumer(read->ready_signal, write->ready_signal);
}

void VHDLBackend::generatePhiNodeAssignment(std::string tmpVar, Value *op) {
  debug_print("generatePhiNodeAssignment(" << tmpVar << ", " << op << ")");
  return_if_dry_run();
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

  ChannelP read = this->read(value);

  if (read->direction == ChannelDirection::CONSTANT_OUT) {
    *op << "   " << remembered_write->valid_signal << " <= '1';\n"
        << "   " << remembered_write->data_signal  << " <= " << read->constant << ";\n";
  } else {
    *op << "   remember_" << remembered->name << " : process(aclk)\n"
        << "   begin\n"
        << "      if reset = '1' then\n"
        << "         " << remembered_write->valid_signal << " <= '0';\n"
        << "         " << remembered_write->data_signal  << " <= (others => '0');\n"
        << "      elsif rising_edge(aclk) and " << read->valid_signal << " = '1' then\n"
        << "         " << remembered_write->valid_signal << " <= " << read->valid_signal << ";\n"
        << "         " << remembered_write->data_signal  << " <= " << read->data_signal  << ";\n"
        << "      end if;\n"
        << "   end process;\n";

    ready_signals->addConsumer(read->ready_signal, "'1'");
  }

  return remembered;
}

void VHDLBackend::generatePhiNode(ValueStorageP target, Value* condition, ValueStorageP trueValue, ValueStorageP falseValue) {
  debug_print("generatePhiNode(...)");
  return_if_dry_run();

  // make sure we cannot miss the valid signal
  ChannelP condition_rem  = remember(vs_factory->get(condition))->getReadChannel(op.get());
  ChannelP trueValue_rem  = remember(trueValue)->getReadChannel(op.get());
  ChannelP falseValue_rem = remember(falseValue)->getReadChannel(op.get());

  ChannelP target_w = target->getWriteChannel(op.get());

  *op << "   " << target_w->valid_signal << " <= " << trueValue_rem->valid_signal
          << " WHEN " << condition_rem->valid_signal << " = '1' and " << condition_rem->data_signal << "(0) = '1' ELSE\n"
      << "      " << falseValue_rem->valid_signal
          << " WHEN " << condition_rem->valid_signal << " = '1' and " << condition_rem->data_signal << "(0) = '0' ELSE\n"
      << "      '0';\n";
  *op << "   " << target_w->data_signal << " <= " << trueValue_rem->data_signal
          << " WHEN " << condition_rem->valid_signal << " = '1' and " << condition_rem->data_signal << "(0) = '1' ELSE\n"
      << "      " << falseValue_rem->data_signal
          << " WHEN " << condition_rem->valid_signal << " = '1' and " << condition_rem->data_signal << "(0) = '0' ELSE\n"
      << "      (others => 'X');\n";
}


void VHDLBackend::generateReturn(Value *retVal) {
  debug_print("generateReturn(" << retVal << ")");
  return_if_dry_run();
  
  ValueStorageP retVal_vs = vs_factory->get(retVal);
  ChannelP ch1 = Channel::make_output("return", retVal_vs->width());
  ch1->addTo(op.get());
  ChannelP ch2 = read(retVal_vs);

  ch1->connectToOutput(ch2, op.get(), usedVariableNames, *ready_signals);


  mboxPutWithoutInterface(1, ch1);
  interface_ccode << "return _get_real(1);\n";
}


void VHDLBackend::generateEndOfMethod() {
  debug_print("generateEndOfMethod()");
  return_if_dry_run();

  std::stringstream s;
  ready_signals->outputVHDL(s);
  *op << s.str();

  op->outputVHDL(*stream);


  BOOST_FOREACH(Signal* signal, *op->getIOList()) {
    r_op->addCalculationPort(signal);
  }
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

  // undo hack for C code backend
  if (name == "*status")
    name = "status";

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

void VHDLBackend::generateSelect(std::string tmpVar, Value *condition, Value *targetTrue,
    Value *targetFalse) {
  debug_print("generateSelect(" << tmpVar << ", " << condition << ", " << targetTrue << ", " << targetFalse << ")");
  return_if_dry_run();

  ValueStorageP tmp = vs_factory->getTemporaryVariable(tmpVar);
  vs_factory->makeSelect(tmp, condition, targetTrue, targetFalse, this);
}

ChannelP VHDLBackend::read(ValueStorageP value) {
  ChannelP read_channel = value->getReadChannel(op.get());

  if (value->kind == ValueStorage::GLOBAL_VARIABLE || value->kind == ValueStorage::FUNCTION_PARAMETER) {
    mboxGet(0, read_channel, value);
  }

  return read_channel;
}

void VHDLBackend::mboxGet(unsigned int mbox, ChannelP channel_of_op, ValueStorageP value) {
  mboxGetWithoutInterface(mbox, channel_of_op);

  std::string type;
  switch (value->width()) {
    case 16: // We shouldn't ever get 16, but we do. We use int in that case.
    case 32: type = "int";  break;
    case 64: type = "real"; break;
    default: std::cerr << "width: " << value->width() << "\n"; assert(false);
  }

  interface_ccode << "_put_" << type << "(" << value->ccode << ", " << toString(mbox) << ");\n";
}

void VHDLBackend::mboxPut(unsigned int mbox, ChannelP channel_of_op, ValueStorageP value) {
  mboxPutWithoutInterface(mbox, channel_of_op);

  std::string type;
  switch (value->width()) {
    case 32: type = "int";  break;
    case 64: type = "real"; break;
    default: assert(false);
  }

  interface_ccode << value->ccode << " = _get_" << type << "(" << mbox << ");\n";
}

void VHDLBackend::mboxGetWithoutInterface(unsigned int mbox, ChannelP channel_of_op) {
  // channel_of_op is the input channel of the calculation, so we have to revert its
  // direction to use it as a dummy output channel for the ReconOS FSM.
  ChannelDirection::Direction backup = channel_of_op->direction;
  channel_of_op->direction = (ChannelDirection::Direction) (backup | ChannelDirection::IN);
  r_op->readMbox(mbox, channel_of_op);
  channel_of_op->direction = backup;
}

void VHDLBackend::mboxPutWithoutInterface(unsigned int mbox, ChannelP channel_of_op) {
  // channel_of_op is the output channel of the calculation, so we have to revert its
  // direction to use it as a dummy input channel for the ReconOS FSM.
  ChannelDirection::Direction backup = channel_of_op->direction;
  channel_of_op->direction = (ChannelDirection::Direction) (backup | ChannelDirection::OUT);
  r_op->writeMbox(mbox, channel_of_op);
  channel_of_op->direction = backup;
}
