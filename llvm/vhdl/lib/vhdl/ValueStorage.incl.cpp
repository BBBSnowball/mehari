
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
