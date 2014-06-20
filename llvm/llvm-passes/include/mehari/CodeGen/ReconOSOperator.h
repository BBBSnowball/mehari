#ifndef RECONOS_OPERATOR_H
#define RECONOS_OPERATOR_H

#include "mehari/CodeGen/MyOperator.h"
#include "mehari/CodeGen/Channel.h"

#include <Operator.hpp>
#include <Signal.hpp>

#include <string>
#include <vector>

class ReconOSOperator : public MyOperator {
  ::Operator* calculation;

  UniqueNameSource stateNameGenerator;
  UniqueNameSet usedStateNames;
public:
  ReconOSOperator();
  ~ReconOSOperator();

  void setCalculation(::Operator* calc);
  void addCalculationPort(Signal* sig);
  void instantiateCalculation();

  void addInitialState();

  void addThreadExitState();

  void addReadIterationsState();


  std::string getUniqueStateName(const std::string& suggested_name);

  std::string getUniqueStateName();


  void readMemory(const std::string& state_name, const std::string& ready_condition,
    const std::string& addr, const std::string& len);

  void readMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel);

  void writeMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel);

  void writeMemory(const std::string& state_name,
    const std::string& addr, const std::string& len);

  void addAckState();

  // internal methods

  void stdLibs(std::ostream& o);
  void outputVHDLEntity(std::ostream& o);
  void outputVHDL(std::ostream& o, std::string name);

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

  State& addSequentialState(const std::string& state_name);
  State& addOutOfBandState(const std::string& state_name);
  State& internalAddState(const std::string& state_name);

  static void splitAccessIntoWords(const std::string& data_signal, unsigned width, std::vector<std::string>& parts);
};

#endif /*RECONOS_OPERATOR_H*/
