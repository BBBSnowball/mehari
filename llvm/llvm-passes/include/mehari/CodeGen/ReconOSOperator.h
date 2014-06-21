#ifndef RECONOS_OPERATOR_H
#define RECONOS_OPERATOR_H

#include "mehari/CodeGen/MyOperator.h"
#include "mehari/CodeGen/Channel.h"

#include <Operator.hpp>
#include <Signal.hpp>

#include <string>
#include <vector>
#include <limits.h>

class BasicReconOSOperator : public MyOperator {
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
    const std::string& addr, const std::string& len, unsigned int local_ram_addr);

  void readMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel);

  void writeMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel);

  void writeMemory(const std::string& state_name,
    const std::string& addr, const std::string& len, unsigned int local_ram_addr,
    const std::string& valid_condition, const std::string& set_ready);

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

  void generateCode(std::ostream& stream) const;
};

inline static std::ostream& operator << (std::ostream& stream, const LocalFakeRam& ram) {
  ram.generateCode(stream);
  return stream;
}


class ReconOSOperator : public BasicReconOSOperator {
  LocalFakeRam ram;
public:
  ReconOSOperator();

  void readMemory(const std::string& address, const ChannelP channel);
  void readMbox(unsigned int mbox, const ChannelP channel);
  void writeMbox(unsigned int mbox, const ChannelP channel);
  void writeMemory(const std::string& address, const ChannelP channel);

  // internal methods

  void outputVHDL(std::ostream& o, std::string name);
};

#endif /*RECONOS_OPERATOR_H*/
