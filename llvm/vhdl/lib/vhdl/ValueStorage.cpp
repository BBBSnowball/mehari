#include "mehari/CodeGen/ValueStorage.h"
#include "mehari/CodeGen/DebugPrint.h"

#include "mehari/utils/ContainerUtils.h"

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/foreach.hpp>

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <stdio.h>

using namespace llvm;
using namespace ChannelDirection;


ValueStorage::ValueStorage() : type(NULL) { }

unsigned int ValueStorage::width() const {
  unsigned int w = _widthWithoutErrorChecks();
  assert(w != 0);
  return w;
}

unsigned int ValueStorage::_widthWithoutErrorChecks() const {
  return elementType()->getPrimitiveSizeInBits();
}

Type* ValueStorage::elementType() const {
  //TODO We sometimes have one level of pointers too many, so we remove it here.
  //     However, we should rather fix the problem that causes this.
  Type* type = this->type;
  assert(type);

  if (isa<SequentialType>(type))
    type = type->getSequentialElementType();

  return type;
}

void ValueStorage::initWithChannels(ChannelP channel_read, ChannelP channel_write) {
  assert(!channel_read  || ChannelDirection::matching_direction(channel_read ->direction, OUT));
  assert(!channel_write || ChannelDirection::matching_direction(channel_write->direction, IN ));

  this->kind = CHANNEL;
  this->channel_read  = channel_read;
  this->channel_write = channel_write;
}

// Call this, if you write a value to channel_write.
void ValueStorage::replaceBy(ChannelP channel_read) {
  this->channel_read = channel_read;
}

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


std::ostream& operator <<(std::ostream& stream, ValueStorage::Kind kind) {
  switch (kind) {
    case ValueStorage::FUNCTION_PARAMETER:
      return stream << "FUNCTION_PARAMETER";
    case ValueStorage::TEMPORARY_VARIABLE:
      return stream << "TEMPORARY_VARIABLE";
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
    unsigned int width = vs._widthWithoutErrorChecks();
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


IfBranch::IfBranch(Value* condition, bool whichBranch, IfBranchP parent, BasicBlock* bb)
    : condition(condition), whichBranch(whichBranch), parent(parent) {
  basic_blocks.insert(bb);
}

ValueStorageP IfBranch::get(ValueStorageP vs) {
  if (ValueStorageP* x = getValueOrNull(temporaries, vs))
    return *x;
  else if (parent)
    return parent->get(vs);
  else
    return vs;
}


ValueStorageFactory::ValueStorageFactory() : tmpVarNameGenerator("t"), last_instruction_was_branch(false) { }

void ValueStorageFactory::clear() {
  by_llvm_value.clear();
  by_name.clear();
  by_index.clear();
  tmpVarNameGenerator.reset();
  current_if_branch = IfBranchP((IfBranch*)NULL);
  if_branches_for_instruction.clear();
  last_instruction_was_branch = false;
}

ValueStorageP ValueStorageFactory::makeParameter(Value* value, const std::string& name) {
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

ValueStorageP ValueStorageFactory::makeTemporaryVariable(Value* value) {
  assert(!contains(by_llvm_value, value));

  ValueStorageP x = makeAnonymousTemporaryVariable(value);

  by_llvm_value[value] = x;
  by_name[x->name] = x;

  return x;
}

ValueStorageP ValueStorageFactory::makeAnonymousTemporaryVariable(Value* value) {
  return makeAnonymousTemporaryVariable(value->getType());
}

ValueStorageP ValueStorageFactory::makeAnonymousTemporaryVariable(Type* type) {
  ValueStorageP x(new ValueStorage());
  x->kind = ValueStorage::TEMPORARY_VARIABLE;
  x->name = tmpVarNameGenerator.next();
  x->type = type;

  return x;
}

ValueStorageP ValueStorageFactory::getOrCreateTemporaryVariable(Value* value) {
  if (ValueStorageP* var = getValueOrNull(by_llvm_value, value))
    return *var;
  else
    return makeTemporaryVariable(value);
}

ValueStorageP ValueStorageFactory::getTemporaryVariable(const std::string& name) {
  if (ValueStorageP* var = getValueOrNull(by_name, name))
    return *var;
  else {
    std::cerr << "ERROR: Couldn't find the variable '" << name << "' in ValueStorageFactory " << this << "!" << std::endl;
    assert(false);
  }
}

ValueStorageP ValueStorageFactory::get(Value* value) {
  ValueStorageP vs = getWithoutIf(value);
  if (current_if_branch)
    vs = current_if_branch->get(vs);
  debug_print("ValueStorageFactory::get(" << value << ") -> " << vs);
  assert(vs);
  return vs;
}

void ValueStorageFactory::set(Value* target, ValueStorageP source) {
  assert(!contains(by_llvm_value, target));

  by_llvm_value[target] = source;
}

ValueStorageP ValueStorageFactory::getForWriting(Value* value) {
  ValueStorageP vs = getWithoutIf(value);

  if (!current_if_branch)
    return vs;

  if (ValueStorageP* x = getValueOrNull(current_if_branch->temporaries, vs)) {
    // There already is a variable for this ValueStorage, which
    // means that we are setting it twice. I think, this shouldn't
    // happen.
    assert(false);

    return *x;
  } else {
    ValueStorageP vs_for_branch = makeAnonymousTemporaryVariable(value);
    current_if_branch->temporaries[vs] = vs_for_branch;
    return vs_for_branch;
  }
}


void ValueStorageFactory::makeUnconditionalBranch(Instruction *target) {
  addIfBranchFor(target, current_if_branch);
  if (current_if_branch)
    current_if_branch->basic_blocks.insert(target->getParent());
  current_if_branch = IfBranchP((IfBranch*)NULL);
  last_instruction_was_branch = true;
}

void ValueStorageFactory::makeConditionalBranch(Value *condition, Instruction *targetTrue, Instruction *targetFalse) {
  addIfBranchFor(targetTrue,  IfBranchP(new IfBranch(condition, true,  current_if_branch, targetTrue ->getParent())));
  addIfBranchFor(targetFalse, IfBranchP(new IfBranch(condition, false, current_if_branch, targetFalse->getParent())));
  current_if_branch = IfBranchP((IfBranch*)NULL);
  last_instruction_was_branch = true;
}

void ValueStorageFactory::beforeInstruction(llvm::Instruction* instr, PhiNodeSink* sink) {
  bool is_first_instruction_in_basic_block = &instr->getParent()->front() == instr;
  bool handle_phinode = isa<PHINode>(instr) && is_first_instruction_in_basic_block;

  if (std::vector<IfBranchP>* branches = getValueOrNull(if_branches_for_instruction, instr)) {
    if (!last_instruction_was_branch)
      // I think this won't happen because LLVM would generate a branch anyway.
      branches->push_back(current_if_branch);

    if (branches->size() == 1)
      current_if_branch = branches->front();
    else
      combineIfBranches(*branches, instr, sink);

    if (handle_phinode) {
      handlePhiNodes(instr->getParent(), *branches, sink);
    }
  } else {
    // If the last instruction was a branch, this instruction must be in a different path.
    // Therefore, we need some branching information (i.e. we should be in the other branch
    // of this if statement).
    assert(!last_instruction_was_branch);

    // We haven't saved any branching information for this statement, so it isn't a branch
    // target which means that a phi node doesn't make much sense here.
    assert(!handle_phinode);
  }

  last_instruction_was_branch = false;
}

ValueStorageP ValueStorageFactory::getWithoutIf(Value* value) {
  ValueStorageP vs = by_llvm_value[value];
  if (!vs)
    vs = get2(value);
  return vs;
}

void ValueStorageFactory::addIfBranchFor(Instruction* target, IfBranchP branch) {
  if_branches_for_instruction[target].push_back(branch);
}

void ValueStorageFactory::combineIfBranches(std::vector<IfBranchP>& branches,
    llvm::Instruction* firstPhiInstruction, PhiNodeSink* sink) {
  debug_print("combineIfBranches(...)");

  // We only implement the simplest case: There is one pair of branches.
  // In the more complex cases, we have to find pairs of branches and also combine the results.
  assert(branches.size() == 2);
  combineIfBranches(branches[0], branches[1], firstPhiInstruction, sink);
}

void ValueStorageFactory::combineIfBranches(IfBranchP a, IfBranchP b,
    llvm::Instruction* firstPhiInstruction, PhiNodeSink* sink) {
  assert(a->condition   == b->condition);
  assert(a->whichBranch != b->whichBranch);
  assert(a->parent      == b->parent);

  std::set<ValueStorageP> vars;
  //vars.insert(mapKeys(a->temporaries).begin(), mapKeys(a->temporaries).end());
  //vars.insert(mapKeys(b->temporaries).begin(), mapKeys(b->temporaries).end());
  boost::copy(a->temporaries | boost::adaptors::map_keys, set_inserter(vars));
  boost::copy(b->temporaries | boost::adaptors::map_keys, set_inserter(vars));

  IfBranchP parent = a->parent;
  IfBranchP trueBranch, falseBranch;
  if (a->whichBranch) {
    trueBranch = a;
    falseBranch = b;
  } else {
    trueBranch = b;
    falseBranch = a;
  }

  BOOST_FOREACH(ValueStorageP var, vars) {
    ValueStorageP trueValue, falseValue, valueInParent;

    valueInParent = var;
    if (parent)
      valueInParent = parent->get(valueInParent);

    trueValue  = trueBranch->get(var);
    falseValue = falseBranch->get(var);

    sink->generatePhiNode(valueInParent, a->condition, trueValue, falseValue);
  }
}

void ValueStorageFactory::handlePhiNodes(BasicBlock* bb, std::vector<IfBranchP>& branches, PhiNodeSink* sink) {
  std::map<BasicBlock*, IfBranchP> branch_by_bb;
  BOOST_FOREACH(IfBranchP branch, branches) {
    BOOST_FOREACH(BasicBlock* bb2, branch->basic_blocks) {
      if (bb == bb2)
        // ignore current basic block because phi nodes can only refer to previous blocks
        continue;

      assert(!contains(branch_by_bb, bb2));

      branch_by_bb[bb2] = branch;
    }
  }

  bool only_phi_nodes_until_now = true;
  BOOST_FOREACH(Instruction& instr, *bb) {
    if (PHINode* phi = dyn_cast<PHINode>(&instr)) {
      // All phi nodes should be at the start of the block.
      assert(only_phi_nodes_until_now);

      if (false) {
        unsigned num = phi->getNumIncomingValues();
        for (unsigned i=0;i<num;i++) {
          std::cout << "incoming from " << phi->getIncomingBlock(i) << ": " << phi->getIncomingValue(i) << std::endl;
        }
        BOOST_FOREACH(IfBranchP branch, branches) {
          std::cout << "branch: " << &*branch << std::endl;
          if (branch) {
            BOOST_FOREACH(BasicBlock* bb2, branch->basic_blocks) {
              std::cout << "  with basic block: " << bb2 << std::endl;
            }
          }
        }
      }

      // only the simplest case is supported: two incoming values that match the branches
      assert(phi->getNumIncomingValues() == 2);

      IfBranchP a = branch_by_bb[phi->getIncomingBlock(0)];
      IfBranchP b = branch_by_bb[phi->getIncomingBlock(1)];
      assert(a);
      assert(b);
      Value* a_value = phi->getIncomingValue(0);
      Value* b_value = phi->getIncomingValue(1);

      assert(a->parent == b->parent);
      IfBranchP parent = a->parent;

      assert(a->condition == b->condition);
      assert(a->whichBranch != b->whichBranch);
      Value* condition = a->condition;

      IfBranchP trueBranch, falseBranch;
      Value *trueValue, *falseValue;
      if (a->whichBranch) {
        trueBranch = a;
        falseBranch = b;
        trueValue = a_value;
        falseValue = b_value;
      } else {
        trueBranch = b;
        falseBranch = a;
        trueValue = b_value;
        falseValue = a_value;
      }

      ValueStorageP trueVS  = trueBranch ->get(this->get(trueValue));
      ValueStorageP falseVS = falseBranch->get(this->get(falseValue));

      ValueStorageP target = getOrCreateTemporaryVariable(phi);
      sink->generatePhiNode(target, condition, trueVS, falseVS);
    } else
      only_phi_nodes_until_now = false;
  }
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
    std::stringstream s;
    s << "std_logic_vector(to_unsigned(" << constant << ", " << width << "))";
    constant = s.str();
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
