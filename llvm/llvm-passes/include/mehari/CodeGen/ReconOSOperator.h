#ifndef RECONOS_OPERATOR_H
#define RECONOS_OPERATOR_H

#include "mehari/CodeGen/MyOperator.h"
#include "mehari/CodeGen/Channel.h"
#include "mehari/CodeGen/ValueStorage.h"

#include <Operator.hpp>
#include <Signal.hpp>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <limits.h>

class BasicReconOSOperator : public MyOperator {
protected:
  ::Operator* calculation;

  UniqueNameSource stateNameGenerator;
  UniqueNameSet usedStateNames;
public:
  BasicReconOSOperator();
  ~BasicReconOSOperator();

  void setCalculation(::Operator* calc);
  void addCalculationPort(Signal* sig);
  void instantiateCalculation();

  void addInitialState();

  void addThreadExitState();

  void addReadIterationsState();


  std::string getUniqueStateName(const std::string& suggested_name);

  std::string getUniqueStateName();


  void readMemory(const std::string& state_name, const std::string& ready_condition,
    const std::string& addr, const std::string& len, unsigned int local_ram_addr,
    unsigned int state_pos = UINT_MAX);

  void readMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel);

  void writeMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel);

  void writeMemory(const std::string& state_name,
    const std::string& addr, const std::string& len, unsigned int local_ram_addr,
    const std::string& valid_condition, const std::string& set_ready);

  void semWait(const std::string& state_name, unsigned int sem);
  void semPost(const std::string& state_name, unsigned int sem);

  void addAckState();

  // internal methods

  void stdLibs(std::ostream& o);
  void outputVHDLEntity(std::ostream& o);
  void outputVHDL(std::ostream& o, std::string name);

  static void splitAccessIntoWords(const std::string& data_signal, unsigned width, std::vector<std::string>& parts);

protected:
  void outputStateDeclarations(std::ostream& o);
  void outputStatemachine(std::ostream& o);

  static bool isValidOfInputSignal (Signal* sig);
  static bool isReadyOfOutputSignal(Signal* sig);


  struct State {
    std::string name;
    std::string comment;
    std::ostringstream vhdl;

    State* nextSequentialState;

    inline State() : nextSequentialState(NULL) { }

    inline State(const State& state) {
      *this = state;
    }

    inline const State& operator = (const State& state) {
      this->name = state.name;
      this->comment = state.comment;
      this->vhdl.str(state.vhdl.str());

      return state;
    }

    inline State& addComment(const std::string& comment) { this->comment = comment; return *this; }
  };
  std::vector<State*> states;
  std::vector<State*> sequential_states;
  std::map<std::string, State*> states_by_name;

  State& addSequentialState(const std::string& state_name, unsigned int pos = UINT_MAX);
  State& addOutOfBandState(const std::string& state_name);
  State& internalAddState(const std::string& state_name, unsigned int pos = UINT_MAX);
};


class LocalFakeRam {
  struct MemoryWordInfo {
    ChannelP channel;
    std::string part;
    bool isLastPart;
  };
  std::vector<MemoryWordInfo> channels;
public:
  LocalFakeRam();

  unsigned int addChannel(ChannelP channel);
  unsigned int getNextAddress();

  void generateCode(std::ostream& stream) const;
};

inline static std::ostream& operator << (std::ostream& stream, const LocalFakeRam& ram) {
  ram.generateCode(stream);
  return stream;
}


class ReconOSOperator : public BasicReconOSOperator {
  LocalFakeRam ram;

  struct ExternalValue {
    ValueStorage::Kind kind;
    std::string name;
    ValueStorageP vs;

    inline bool operator <(const ExternalValue& other) const {
      return kind << other.kind || (kind == other.kind && name < other.name);
    }
  };
  std::set<ExternalValue> externalValues;

  std::set<ValueStorageP> already_read;
public:
  ReconOSOperator();

  void readMbox(unsigned int mbox, const ChannelP channel);
  void writeMbox(unsigned int mbox, const ChannelP channel);

  void readMemory(ValueStorageP vs);
  void writeMemory(ValueStorageP vs);

  void readMemory(const std::string& address, const ChannelP channel);
  void writeMemory(const std::string& address, const ChannelP channel);

  // internal methods

  void outputVHDL(std::ostream& o, std::string name);
protected:
  void addReadParamsState();
};

#endif /*RECONOS_OPERATOR_H*/
