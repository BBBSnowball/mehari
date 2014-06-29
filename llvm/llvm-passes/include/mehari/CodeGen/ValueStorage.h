#ifndef VALUE_STORAGE_H
#define VALUE_STORAGE_H

#include "mehari/CodeGen/Channel.h"
#include "mehari/utils/UniqueNameSource.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <ios>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

typedef boost::shared_ptr<struct ValueStorage> ValueStorageP;

struct ValueStorage {
  enum Kind {
    FUNCTION_PARAMETER,
    TEMPORARY_VARIABLE,
    GLOBAL_VARIABLE,
    CHANNEL
  };
  Kind kind;
  std::string name;
  llvm::Type* type;
  ValueStorageP parent;
  std::string offset;
  std::string ccode;

  ValueStorage();

  unsigned int width() const;

  unsigned int _widthWithoutErrorChecks() const;

  llvm::Type* elementType() const;

private:
  ChannelP channel_read, channel_write;

public:
  ChannelP getReadChannel(MyOperator* op);
  ChannelP getWriteChannel(MyOperator* op);
  ChannelP getExternalWriteChannel();
  bool hasBeenWrittenTo();

  void initWithChannels(ChannelP channel_read, ChannelP channel_write, llvm::Type* type);

  // Call this, if you write a value to channel_write.
  void replaceBy(ChannelP channel_read);
};

std::ostream& operator <<(std::ostream& stream, ValueStorage::Kind kind);

std::ostream& operator <<(std::ostream& stream, const ValueStorage& vs);

std::ostream& operator <<(std::ostream& stream, ValueStorageP vs);


typedef boost::shared_ptr<struct IfBranch> IfBranchP;

struct IfBranch {
  llvm::Value* condition;
  bool whichBranch;
  IfBranchP parent;

  std::map<ValueStorageP, ValueStorageP> temporaries;

  std::set<llvm::BasicBlock*> basic_blocks;

  IfBranch(llvm::Value* condition, bool whichBranch, IfBranchP parent, llvm::BasicBlock* bb);
  ValueStorageP get(ValueStorageP vs);
};


struct PhiNodeSink {
  virtual void generatePhiNode(ValueStorageP target, llvm::Value* condition, ValueStorageP trueValue, ValueStorageP falseValue) =0;
};


class ValueStorageFactory {
  std::map<llvm::Value*, ValueStorageP> by_llvm_value;
  std::map<std::string,  ValueStorageP> by_name;

  struct ValueAndIndex {
    ValueStorageP value;
    std::string index;

    inline ValueAndIndex(ValueStorageP value, const std::string& index) : value(value), index(index) { }
    inline ValueAndIndex(ValueStorageP value) : value(value) { }

    inline bool operator <(const ValueAndIndex& other) const {
      return value < other.value || (value == other.value && index < other.index);
    }
  };
  std::map<ValueAndIndex, ValueStorageP> by_index;

  UniqueNameSource tmpVarNameGenerator;

  IfBranchP current_if_branch;
  std::map<llvm::Instruction*, std::vector<IfBranchP> > if_branches_for_instruction;
  bool last_instruction_was_branch;
public:
  ValueStorageFactory();

  void clear();

  ValueStorageP makeParameter(llvm::Value* value, const std::string& name);

  ValueStorageP makeTemporaryVariable(llvm::Value* value);

  ValueStorageP makeAnonymousTemporaryVariable(llvm::Value* value);

  ValueStorageP makeAnonymousTemporaryVariable(llvm::Type* type);

  ValueStorageP getOrCreateTemporaryVariable(llvm::Value* value);

  ValueStorageP getTemporaryVariable(const std::string& name);

  ValueStorageP getGlobalVariable(llvm::Value* v);
  ValueStorageP getGlobalVariable(const std::string& name, llvm::Type* type);
  ValueStorageP getAtConstIndex(ValueStorageP ptr, llvm::Value* index);
  ValueStorageP getAtConstIndex(ValueStorageP ptr, std::string str_index);
  ValueStorageP getConstant(const std::string& constant, unsigned int width, llvm::Type* type);

  ValueStorageP get(llvm::Value* value);

  void set(llvm::Value* target, ValueStorageP source);

  ValueStorageP getForWriting(llvm::Value* value);


  void makeUnconditionalBranch(llvm::Instruction *target);

  void makeConditionalBranch(llvm::Value *condition, llvm::Instruction *targetTrue, llvm::Instruction *targetFalse);

  void makeSelect(ValueStorageP target, llvm::Value *condition, llvm::Value *trueValue,
    llvm::Value *falseValue, PhiNodeSink* sink);

  void beforeInstruction(llvm::Instruction* instr, PhiNodeSink* sink);

private:
  ValueStorageP get2(llvm::Value* value);

  ValueStorageP getWithoutIf(llvm::Value* value);

  void addIfBranchFor(llvm::Instruction* target, IfBranchP branch);

  void combineIfBranches(std::vector<IfBranchP>& branches, llvm::Instruction* firstPhiInstruction, PhiNodeSink* sink);

  void combineIfBranches(IfBranchP a, IfBranchP b, PhiNodeSink* sink);

  void handlePhiNodes(llvm::BasicBlock* bb, std::vector<IfBranchP>& branches, PhiNodeSink* sink);
};

#endif /*VALUE_STORAGE_H*/
