#ifndef CHANNEL_H
#define CHANNEL_H

#include "mehari/utils/UniqueNameSource.h"
#include "mehari/CodeGen/OperatorInfo.h"
#include "mehari/CodeGen/MyOperator.h"
#include "mehari/CodeGen/ReadySignals.h"

#include <string>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "Operator.hpp"

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

typedef boost::shared_ptr<struct Channel> ChannelP;

struct Channel {
  ChannelDirection::Direction direction;
  std::string constant;

  ::Operator* component;
  std::string data_signal, valid_signal, ready_signal;
  unsigned int width;

  static ChannelP make_constant(const std::string& constant, unsigned int width);

  static ChannelP make_port(const std::string& name, ChannelDirection::Direction direction, unsigned int width);
  static ChannelP make_input(const std::string& name, unsigned int width);
  static ChannelP make_output(const std::string& name, unsigned int width);
  static ChannelP make_component_port(
      ::Operator* component, const std::string& name, ChannelDirection::Direction direction, const OperatorInfo& op_info,
      unsigned int width = 0);
  static ChannelP make_component_port(
      ::Operator* component, const std::string& name, ChannelDirection::Direction direction,
      unsigned int width = 0);
  static ChannelP make_component_input(::Operator* component, const std::string& name, const OperatorInfo& op_info);
  static ChannelP make_component_input(::Operator* component, const std::string& name);
  static ChannelP make_component_input_lazy(::Operator* component, const std::string& name, unsigned int width);
  static ChannelP make_component_output(::Operator* component, const std::string& name, const OperatorInfo& op_info);
  static ChannelP make_component_output(::Operator* component, const std::string& name);

  static ChannelP make_temp(::Operator* op, const std::string& name, unsigned int width);

  static ChannelP make_variable(::Operator* op, const std::string& name, unsigned int width);

  static ChannelP make_part(const std::string& data_channel_part, unsigned int part_width, ChannelP channel);

  void addTo(::Operator* op);

  void generateSignal(MyOperator* op, UniqueNameSet& usedVariableNames);

private:
  static void outPortMap(MyOperator* op, ::Operator* component, const std::string& port, const std::string& signal,
      UniqueNameSet& usedVariableNames);

  static void outPortMapReadySignal(MyOperator* op, ::Operator* component, const std::string& port, const std::string& signal,
      UniqueNameSet& usedVariableNames, ReadySignals& rsignals);

public:
  void connectToOutput(Channel* ch, MyOperator* op, UniqueNameSet& usedVariableNames, ReadySignals& rsignals);

  void connectToOutput(ChannelP ch, MyOperator* op, UniqueNameSet& usedVariableNames, ReadySignals& rsignals);

  void connectToInput(Channel* ch, MyOperator* op, UniqueNameSet& usedVariableNames, ReadySignals& rsignals);

  void connectToInput(ChannelP ch, MyOperator* op, UniqueNameSet& usedVariableNames, ReadySignals& rsignals);
};

#endif /*CHANNEL_H*/
