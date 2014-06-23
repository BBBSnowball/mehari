#include "mehari/CodeGen/ReconOSOperator.h"
#include "mehari/utils/StringUtils.h"
#include "mehari/utils/ContainerUtils.h"

#include <assert.h>
#include <boost/foreach.hpp>
#include <ios>
#include <iomanip>


BasicReconOSOperator::BasicReconOSOperator() : calculation(NULL), stateNameGenerator("STATE") {
  addInput ("OSIF_FIFO_Sw2Hw_Data", 32);
  addInput ("OSIF_FIFO_Sw2Hw_Fill", 16);
  addInput ("OSIF_FIFO_Sw2Hw_Empty");
  addOutput("OSIF_FIFO_Sw2Hw_RE");

  addOutput("OSIF_FIFO_Hw2Sw_Data", 32);
  addInput ("OSIF_FIFO_Hw2Sw_Rem", 16);
  addInput ("OSIF_FIFO_Hw2Sw_Full");
  addOutput("OSIF_FIFO_Hw2Sw_WE");

  addOutput("MEMIF_FIFO_Hwt2Mem_Data", 32);
  addInput ("MEMIF_FIFO_Hwt2Mem_Rem", 16);
  addInput ("MEMIF_FIFO_Hwt2Mem_Full");
  addOutput("MEMIF_FIFO_Hwt2Mem_WE");

  addInput ("MEMIF_FIFO_Mem2Hwt_Data", 32);
  addInput ("MEMIF_FIFO_Mem2Hwt_Fill", 16);
  addInput ("MEMIF_FIFO_Mem2Hwt_Empty");
  addOutput("MEMIF_FIFO_Mem2Hwt_RE");

  addPort("HWT_Clk",1,1,1,0,0,0,0,0,0,0,0);
  addPort("HWT_Rst",1,1,0,1,0,0,0,0,0,0,0);

  declare("clk", 1);
  declare("rst", 1);
  *this << "   clk <= HWT_Clk;" << std::endl;
  *this << "   rst <= HWT_Rst;" << std::endl;
  *this << std::endl;

  declare("init", 1);

  *this
    << "   -- ReconOS initilization" << std::endl;
    *this
    << "   osif_setup (" << std::endl
    << "     i_osif," << std::endl
    << "     o_osif," << std::endl
    << "     OSIF_FIFO_Sw2Hw_Data," << std::endl
    << "     OSIF_FIFO_Sw2Hw_Fill," << std::endl
    << "     OSIF_FIFO_Sw2Hw_Empty," << std::endl
    << "     OSIF_FIFO_Hw2Sw_Rem," << std::endl
    << "     OSIF_FIFO_Hw2Sw_Full," << std::endl
    << "     OSIF_FIFO_Sw2Hw_RE," << std::endl
    << "     OSIF_FIFO_Hw2Sw_Data," << std::endl
    << "     OSIF_FIFO_Hw2Sw_WE" << std::endl
    << "   );" << std::endl
    << std::endl
    << "   memif_setup (" << std::endl
    << "     i_memif," << std::endl
    << "     o_memif," << std::endl
    << "     MEMIF_FIFO_Mem2Hwt_Data," << std::endl
    << "     MEMIF_FIFO_Mem2Hwt_Fill," << std::endl
    << "     MEMIF_FIFO_Mem2Hwt_Empty," << std::endl
    << "     MEMIF_FIFO_Hwt2Mem_Rem," << std::endl
    << "     MEMIF_FIFO_Hwt2Mem_Full," << std::endl
    << "     MEMIF_FIFO_Mem2Hwt_RE," << std::endl
    << "     MEMIF_FIFO_Hwt2Mem_Data," << std::endl
    << "     MEMIF_FIFO_Hwt2Mem_WE" << std::endl
    << "   );" << std::endl
    << std::endl
    << "   ram_setup (" << std::endl
    << "     i_ram," << std::endl
    << "     o_ram," << std::endl
    << "     o_RAMAddr," << std::endl
    << "     o_RAMWE," << std::endl
    << "     o_RAMData," << std::endl
    << "     i_RAMData" << std::endl
    << "   );" << std::endl
    << std::endl;
}

BasicReconOSOperator::~BasicReconOSOperator() {
  BOOST_FOREACH(State* state, states) {
    delete state;
  }
}

void BasicReconOSOperator::setCalculation(::Operator* calc) {
  assert(!this->calculation);

  this->calculation = calc;

  BOOST_FOREACH(::Signal* sig, *calc->getIOList()) {
    addCalculationPort(sig);
  }
}

void BasicReconOSOperator::addCalculationPort(Signal* sig) {
  assert(this->calculation);
  assert(this->calculation->getSignalByName(sig->getName()) == sig);

  if (sig->isClk())
    inPortMap(calculation, sig->getName(), "clk");
  else if (sig->isRst())
    inPortMap(calculation, sig->getName(), "rst");
  else if (sig->type() == Signal::in) {
    std::string name = sig->getName();
    if (isValidOfInputSignal(sig)) {
      declare(name, sig->width(), sig->isBus());
      std::string component_accepted_the_value = sig->getName() + "_component_accepted_the_value";
      declare(component_accepted_the_value, 1);

      name = name + "_direct";
    }
    declare(name, sig->width(), sig->isBus());
    inPortMap(calculation, sig->getName(), name);
  } else if (sig->type() == Signal::out)
    outPortMap(calculation, sig->getName(), sig->getName());
  else
    assert(false);
}

void BasicReconOSOperator::instantiateCalculation() {
  assert(this->calculation);

  vhdl << instance(calculation, "calculation");
}

BasicReconOSOperator::State& BasicReconOSOperator::addSequentialState(const std::string& state_name, unsigned int pos) {
  State& state = internalAddState(state_name, pos);
  
  if (pos >= sequential_states.size())
    sequential_states.push_back(&state);
  else
    sequential_states.insert(sequential_states.begin()+pos, &state);

  return state;
}

BasicReconOSOperator::State& BasicReconOSOperator::addOutOfBandState(const std::string& state_name) {
  return internalAddState(state_name);
}

BasicReconOSOperator::State& BasicReconOSOperator::internalAddState(const std::string& state_name, unsigned int pos) {
  assert(!contains(states_by_name, state_name));

  State* state = new State();
  state->name = state_name;

  if (pos > states.size())
    states.push_back(state);
  else
    states.insert(states.begin()+pos, state);
  states_by_name[state_name] = state;

  return *state;
}

bool BasicReconOSOperator::isValidOfInputSignal(Signal* sig) {
  return (endsWith(sig->getName(), "_valid") || endsWith(sig->getName(), "_tvalid"))
    && (sig->type() == Signal::in);
}

bool BasicReconOSOperator::isReadyOfOutputSignal(Signal* sig) {
  return (endsWith(sig->getName(), "_ready") || endsWith(sig->getName(), "_tready"))
    && (sig->type() == Signal::in); // ready signal of an output is an input.
}

void BasicReconOSOperator::stdLibs(std::ostream& o) {
  o << "library ieee;" << endl;
  o << "use ieee.std_logic_1164.all;" << endl;
  o << "use ieee.std_logic_unsigned.all;" << endl;
  o << "use ieee.math_real.ALL;" << endl;
  o << "use ieee.numeric_std.all;" << endl;
  o << endl;
  o << "--TODO" << endl;
  o << "--library proc_common_v3_00_a;" << endl;
  o << "--use proc_common_v3_00_a.proc_common_pkg.all;" << endl;
  o << endl;
  o << "library reconos_v3_01_a;" << endl;
  o << "use reconos_v3_01_a.reconos_pkg.all;" << endl;
  o << endl;
}


void BasicReconOSOperator::outputVHDLEntity(std::ostream& o) {
  o << "entity " << uniqueName_ << " is" << endl;

  if (!ioList_.empty()) {
    o << tab << "port ( " << endl;

    Seperator sep(";" + toString(std::endl));
    BOOST_FOREACH(Signal* s, ioList_) {
      o << sep << tab << tab << tab << s->toVHDL();
    }

    o << endl << tab << ");" << endl;
  }

  o << "   attribute SIGIS : string;" << endl;
  BOOST_FOREACH(Signal* s, ioList_) {
    if (s->isClk())
      o << "   attribute SIGIS of " << s->getName() << " : signal is \"Clk\";" << endl;
    if (s->isRst())
      o << "   attribute SIGIS of " << s->getName() << " : signal is \"Rst\";" << endl;
  }

  o << "end entity;" << endl << endl;
}

void BasicReconOSOperator::outputStateDeclarations(std::ostream& o) {
  o << "   type STATE_TYPE is (";
  int i = 0;
  Seperator comma(", ");
  BOOST_FOREACH(State* state, states) {
    o << comma;

    // 4 states per line
    if ((i++ % 4) == 0)
      o << endl << "      ";

    o << "STATE_" << state->name;
  }
  o << ");" << endl;
  o << "   signal state        : STATE_TYPE;" << endl;


  //TODO move to other method or rename this method
  o << "   signal i_osif       : i_osif_t;" << endl;
  o << "   signal o_osif       : o_osif_t;" << endl;
  o << "   signal i_memif      : i_memif_t;" << endl;
  o << "   signal o_memif      : o_memif_t;" << endl;
  o << "   signal i_ram        : i_ram_t;" << endl;
  o << "   signal o_ram        : o_ram_t;" << endl;


  //TODO use those variables.
  o << "   signal addr      : std_logic_vector(31 downto 0);" << endl;
  o << "   signal len       : std_logic_vector(23 downto 0);" << endl;
  o << "   signal ignore    : std_logic_vector(31 downto 0);" << endl;
  o << "   signal o_RAMAddr : std_logic_vector(0 to 31);" << endl;
  o << "   signal o_RAMData : std_logic_vector(0 to 31);" << endl;
  o << "   signal o_RAMWE   : std_logic;" << endl;
  o << "   signal i_RAMData : std_logic_vector(0 to 31);" << endl;
}

void BasicReconOSOperator::outputStatemachine(std::ostream& o) {
  assert(calculation);

  BOOST_FOREACH(Signal* valid_sig, *calculation->getIOList()) {
    if (!isValidOfInputSignal(valid_sig))
      continue;

    Signal* ready_sig = calculation->getSignalByName(replace("valid", "ready", valid_sig->getName()));

    std::string direct_valid_signal = valid_sig->getName() + "_direct";

    std::string component_accepted_the_value = valid_sig->getName() + "_component_accepted_the_value";

    o << "   " << valid_sig->getName() << "_valid: process (clk, rst, " << valid_sig->getName() << ") is" << endl
      << "   begin" << endl
      << "      if init = '1' then" << endl
      << "         " << direct_valid_signal << " <= '0';" << endl
      << "         " << component_accepted_the_value << " <= '0';" << endl
      << "      elsif rising_edge(clk) then" << endl
      << "         if " << component_accepted_the_value << " = '1' then" << endl
      << "            " << direct_valid_signal << " <= '0';" << endl
      << "         elsif " << valid_sig->getName() << " = '1' and " << ready_sig->getName() << " = '1' then" << endl
      << "            " << direct_valid_signal << " <= '1';" << endl
      << "            " << component_accepted_the_value << " <= '1';" << endl
      << "         else" << endl
      << "            " << direct_valid_signal << " <= " << valid_sig->getName() << ";" << endl
      << "         end if;" << endl
      << "      end if;" << endl
      << "   end process;" << endl;
  }

  o << "   -- os and memory synchronisation state machine" << endl
    << "   reconos_fsm: process (clk,rst,o_osif,o_memif,o_ram) is" << endl
    << "     variable done  : boolean;" << endl

    << "   --TODO remove" << endl
    << "     --procedure goto_read(dummy:integer) is" << endl
    << "     --begin" << endl
    << "     --  len   <= conv_std_logic_vector(8 * C_INPUT_SIZE,24);" << endl
    << "     --  addr  <= addr(31 downto 2) & \"00\";" << endl
    << "     --  state <= STATE_READ;" << endl
    << "     --end;" << endl

    << "   begin" << endl
    << "      if rst = '1' then" << endl
    << "          osif_reset(o_osif);" << endl
    << "          memif_reset(o_memif);" << endl
    << "          ram_reset(o_ram);" << endl
    << "          state <= STATE_INIT;" << endl
    << "          done  := False;" << endl
    << "          addr <= (others => '0');" << endl
    << "          len <= (others => '0');" << endl
    << "          init <= '1';" << endl
    << endl;


  BOOST_FOREACH(Signal* sig, *calculation->getIOList()) {
    if (isValidOfInputSignal(sig))
      o << "         " << sig->getName() << " <= '0';" << endl;
    else if (isReadyOfOutputSignal(sig))
      o << "         " << sig->getName() << " <= '0';" << endl;
  }

  o << endl
    << "      elsif rising_edge(clk) then" << endl
    << "         case state is" << endl;


  if (!sequential_states.empty()) {
    State* prevState = sequential_states.back();
    BOOST_FOREACH(State* sig, sequential_states) {
      if (prevState)
        prevState->nextSequentialState = sig;

      prevState = sig;
    }
  }

  BOOST_FOREACH(const State* state, states) {
    if (!state->comment.empty())
      o << prefixAllLines("            -- ", state->comment) << endl;

    o << "            when STATE_" << state->name << " =>" << endl;

    std::string vhdl = state->vhdl.str();
    if (state->nextSequentialState) {
      std::ostringstream s;
      s << "state <= STATE_" << state->nextSequentialState->name << ";";
      vhdl = replace("$next", s.str(), vhdl);
    }

    std::string indent = "               ";
    vhdl = prefixAllLines(indent, vhdl);

    o << vhdl;
    o << endl;
  }

  o << "         end case;" << endl
    << "      end if;" << endl
    << "   end process;" << endl;
}

void BasicReconOSOperator::outputVHDL(std::ostream& o, std::string name) {
  licence(o, copyrightString_);
  stdLibs(o);
  outputVHDLEntity(o);
  newArchitecture(o,name);
  o << buildVHDLComponentDeclarations();  
  o << buildVHDLSignalDeclarations();
  outputStateDeclarations(o);
  beginArchitecture(o);   
  o << buildVHDLRegisters();
  o << vhdl.str();
  outputStatemachine(o);
  endArchitecture(o);
}

void BasicReconOSOperator::addInitialState() {
  State& state = addSequentialState("INIT", 0)
    .addComment("get address via mbox: the data will be copied from this address to the local ram in the next states");
  
  state.vhdl
      << "init <= '1';" << endl
      << "osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(0, 32)), addr, done);" << endl
      << "if done then" << endl
      << "  init <= '0';" << endl
      << "  if (addr = X\"FFFFFFFF\") then" << endl
      << "    state <= STATE_THREAD_EXIT;" << endl
      << "  -- elsif (addr = X\"FFFFFFFE\") then" << endl
      << "  --   state <= STATE_READ;" << endl
      << "  --elsif (addr = X\"FFFFFFFD\") then" << endl
      << "  --  state <= STATE_READ_ITERATIONS;" << endl
      << "  else" << endl
      << "    $next" << endl
      << "  end if;" << endl
      << "end if;" << endl
      << endl;

  BOOST_FOREACH(Signal* sig, *calculation->getIOList()) {
    if (isValidOfInputSignal(sig))
      state.vhdl << sig->getName() << " <= '0';" << endl;
    else if (isReadyOfOutputSignal(sig))
      //state.vhdl << sig->getName() << " <= '0';" << endl;
      //DEBUG
      state.vhdl << sig->getName() << " <= '1';" << endl;
  }
}

void BasicReconOSOperator::addThreadExitState() {
  State state;
  addOutOfBandState("THREAD_EXIT")
    .addComment("thread exit")
    .vhdl
      << "osif_thread_exit(i_osif,o_osif);" << endl;
}

void BasicReconOSOperator::addReadIterationsState() {
  declare("iterations", 32);

  State state;
  addOutOfBandState("READ_ITERATIONS")
    .vhdl
      << "osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(0, 32)), iterations, done);" << endl
      << "if done then" << endl
      << "   $next" << endl
      << endl
      << "   -- we have to subtract 1 because our code is like this:" << endl
      << "   -- while (true) { calc(); if (iterations == 0) break; --iterations; }" << endl
      << "   -- Rational: In the normal case, iterations will always be 0. We" << endl
      << "   --           don't have to think about it. Else, we would have to" << endl
      << "   --           set it to 1 whenever the user sends an address unless" << endl
      << "   --           she has send a different value for iterations." << endl
      << "   iterations <= iterations - X\"00000001\";" << endl
      << "end if;" << endl;
}

std::string BasicReconOSOperator::getUniqueStateName(const std::string& suggested_name) {
  return usedStateNames.makeUnique(suggested_name);
}

std::string BasicReconOSOperator::getUniqueStateName() {
  return getUniqueStateName(stateNameGenerator.next());
}

std::string addressToVHDLString(unsigned int local_ram_addr) {
  std::ostringstream s;
  s << "X\"" << std::hex << std::setfill('0') << std::setw(8) << local_ram_addr << "\"";
  return s.str();
}

void BasicReconOSOperator::readMemory(const std::string& state_name, const std::string& ready_condition,
    const std::string& addr, const std::string& len, unsigned int local_ram_addr,
    unsigned int state_pos) {
  std::string local_addr = addressToVHDLString(local_ram_addr);

  if (!ready_condition.empty()) {
    addSequentialState(getUniqueStateName(state_name + "_wait"), state_pos)
      .vhdl
        << "if " << ready_condition << " then" << endl
        << "   memif_read(i_ram,o_ram,i_memif,o_memif," << addr << "," << local_addr << "," << len << ",done);" << endl
        << "   $next" << endl
        << "end if;";
    if (state_pos != UINT_MAX)
      state_pos++;
  }

  addSequentialState(state_name, state_pos)
    .vhdl
      << "memif_read(i_ram,o_ram,i_memif,o_memif," << addr << "," << local_addr << "," << len << ",done);" << endl
      << "if done then" << endl
      << "  $next" << endl
      << "end if;" << endl;
}

void BasicReconOSOperator::readMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel) {
  assert(channel && ChannelDirection::matching_direction(ChannelDirection::IN, channel->direction));

  unsigned width = channel->width;
  std::vector<std::string> parts;
  splitAccessIntoWords(channel->data_signal, width, parts);

  BOOST_FOREACH(const std::string& part, parts) {
    State& state = addSequentialState(getUniqueStateName(state_name));

    state.vhdl
      << "osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(" << mbox << ", 32)), "
        << part << ", done);" << endl
      << "if done then" << endl;

    if (&part == &parts.back()) {
      state.vhdl << "   " << channel->valid_signal << " <= '1';" << endl;
    }

    state.vhdl
      << "  $next" << endl
      << "end if;" << endl;
  }
}

void BasicReconOSOperator::writeMbox(const std::string& state_name, unsigned int mbox, const ChannelP channel) {
  assert(channel && ChannelDirection::matching_direction(ChannelDirection::OUT, channel->direction));

  unsigned width = channel->width;
  std::vector<std::string> parts;
  splitAccessIntoWords(channel->data_signal, width, parts);

  if (!channel->valid_signal.empty()) {
    addSequentialState(getUniqueStateName(state_name + "_wait"))
      .vhdl
        << channel->ready_signal << " <= '1';" << endl
        << "if " << channel->valid_signal << " = '1' then" << endl
        //TODO put value in register!
        << "   osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(" << mbox << ", 32)), "
          << parts.front() << ", ignore, done);" << endl
        << "   $next" << endl
        << "end if;";
  }

  BOOST_FOREACH(const std::string& part, parts) {
    State state;
    addSequentialState(getUniqueStateName(state_name))
      .vhdl
        << "osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(" << mbox << ", 32)), "
          << part << ", ignore, done);" << endl
        << "if done then" << endl
        << "  " << channel->ready_signal << " <= '0';" << endl
        << "  $next" << endl
        << "end if;" << endl;
  }
}

void BasicReconOSOperator::splitAccessIntoWords(const std::string& data_signal, unsigned width, std::vector<std::string>& parts) {
  for (unsigned i=0;i<width || i==0;i+=32) {
    std::ostringstream s;
    if (width - i >= 32) {
      s << data_signal << "(" << (i+31) << " downto " << i << ")";
    } else {
      s << '"';
      for (unsigned j=0;j<32-(width-i);j++)
        s << '0';
      s << '"';
      if (width > 0)
        s << " & " << data_signal << "(" << (width-1) << " downto " << i << ")";
    }

    parts.push_back(s.str());
  }
}

void BasicReconOSOperator::writeMemory(const std::string& state_name,
    const std::string& addr, const std::string& len, unsigned int local_ram_addr,
    const std::string& valid_condition, const std::string& set_ready) {
  std::string local_addr = addressToVHDLString(local_ram_addr);

  if (!valid_condition.empty()) {
    addSequentialState(getUniqueStateName(state_name + "_wait"))
      .vhdl
        << replace("$value", "'1'", set_ready)
        << "if " << valid_condition << " then" << endl
        << "   memif_write(i_ram,o_ram,i_memif,o_memif," << local_addr << "," << addr << "," << len << ",done);" << endl
        << "   $next" << endl
        << "end if;";
  }

  addSequentialState(state_name)
    .vhdl
      << "memif_write(i_ram,o_ram,i_memif,o_memif," << local_addr << "," << addr << "," << len << ",done);" << endl
      << "if done then" << endl
      << prefixAllLines("  ", replace("$value", "'0'", set_ready))
      << "  $next" << endl
      << "end if;" << endl;
}

void BasicReconOSOperator::addAckState() {
  addSequentialState(getUniqueStateName("FINISH"))
    .vhdl
      << "--if iterations = X\"00000000\" then" << endl
      << "  osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(1, 32)), addr, ignore, done);" << endl
      << "  if done then" << endl
      << "    $next" << endl
      << "  end if;" << endl
      << "--else" << endl
      << "--  iterations <= iterations - X\"00000001\";" << endl
      << "--  goto_read(0);" << endl
      << "--end if;" << endl;
}


LocalFakeRam::LocalFakeRam() { }

unsigned int LocalFakeRam::addChannel(ChannelP channel) {
  unsigned int addr = channels.size();

  std::vector<std::string> parts;
  BasicReconOSOperator::splitAccessIntoWords(channel->data_signal, channel->width, parts);

  BOOST_FOREACH(const std::string& part, parts) {
    MemoryWordInfo mwi = { channel, part, (part == parts.back()) };
    channels.push_back(mwi);
  }

  return addr;
}

unsigned int LocalFakeRam::getNextAddress() {
  return channels.size();
}

void LocalFakeRam::generateCode(std::ostream& stream) const {
  stream
    << "  local_fake_ram_ctrl : process (clk) is\n"
    << "  begin\n"
    << "    if (rising_edge(clk)) then\n"
    << "      if (o_RAMWE = '1') then\n"
    << "        case conv_integer(o_RAMAddr) is\n";

  unsigned int addr = 0;
  BOOST_FOREACH(const MemoryWordInfo& mwi, channels) {
    if (ChannelDirection::matching_direction(mwi.channel->direction, ChannelDirection::IN)) {
      stream << "          when " << std::setw(2) << addr << " =>\n"
             << "            " << mwi.part << " <= o_RAMData";
      if ((mwi.channel->width%32) != 0 && mwi.isLastPart)
        stream << "(" << (mwi.channel->width%32-1) << " downto 0)";
      stream << ";\n";
      if (!mwi.isLastPart)
        stream << "            " << mwi.channel->valid_signal << " <= '1';\n";
    }

    addr++;
  }

  stream
    << "          when others => -- do nothing\n"
    << "        end case;\n"
    << "      else\n"
    << "        case conv_integer(o_RAMAddr) is\n";

  addr = 0;
  BOOST_FOREACH(const MemoryWordInfo& mwi, channels) {
    if (ChannelDirection::matching_direction(mwi.channel->direction, ChannelDirection::OUT)) {
      stream << "          when " << std::setw(2) << addr << " =>\n"
             << "            if " << mwi.channel->valid_signal << " = '1' then\n"
             << "              i_RAMData <= ";

      if ((mwi.channel->width%32) != 0 && mwi.isLastPart) {
        stream << "\"";
        for (unsigned int i=0;i<32-mwi.channel->width%32;i++)
          stream << '0';
        stream << "\" & ";
      }
      stream << mwi.part << ";\n";

      stream  << "            else\n"
              << "              i_RAMData <= (others => 'U');\n"
              << "            end if;\n";
    }

    addr++;
  }

  stream
    << "          when others => i_RAMData <= (others => 'U');\n"
    << "        end case;\n"
    << "      end if;\n"
    << "    end if;\n"
    << "  end process;\n";
}


void ReconOSOperator::readMbox(unsigned int mbox, const ChannelP channel) {
  BasicReconOSOperator::readMbox(getUniqueStateName("MBOX_READ_" + channel->data_signal), mbox, channel);
}

void ReconOSOperator::writeMbox(unsigned int mbox, const ChannelP channel) {
  BasicReconOSOperator::writeMbox(getUniqueStateName("MBOX_WRITE_" + channel->data_signal), mbox, channel);
}

void ReconOSOperator::readMemory(const std::string& address, const ChannelP channel) {
  BasicReconOSOperator::readMemory(getUniqueStateName("MEM_READ_" + channel->data_signal), channel->ready_signal + " = '1'",
    address, "std_logic_vector(to_unsigned(" + toString(channel->width/8) + ", 24))",
    ram.addChannel(channel));
}

void ReconOSOperator::writeMemory(const std::string& address, const ChannelP channel) {
  BasicReconOSOperator::writeMemory(getUniqueStateName("MEM_WRITE_" + channel->data_signal), address,
    "std_logic_vector(to_unsigned(" + toString(channel->width/8) + ", 24))",
    ram.addChannel(channel),
    channel->valid_signal + " = '1'", channel->ready_signal + " <= $value;\n");
}

ReconOSOperator::ReconOSOperator() {
}

void ReconOSOperator::outputVHDL(std::ostream& o, std::string name) {
  instantiateCalculation();

  addInitialState();
  addReadParamsState();
  addThreadExitState();

  addAckState();

  *this << ram;

  BasicReconOSOperator::outputVHDL(o, name);
}

void ReconOSOperator::readMemory(ValueStorageP vs) {
  if (contains(already_read, vs))
    return;
  already_read.insert(vs);

  if (vs->parent) {
    readMemory(vs->parent);
    const std::string& address_variable = vs->parent->name + "_in_data";

    std::ostringstream address;
    address << address_variable << " + std_logic_vector(to_unsigned(" << toString(vs->offset) << "*" << (vs->width()/8) << ", 32))";

    readMemory(address.str(), Channel::make_component_input_lazy(calculation, vs->name, vs->width()));
  } else {
    // We cannot read this value because we don't even know its address. Therefore, it must
    // be passed in with the parameters.

    ExternalValue ev = { vs->kind, vs->name, vs };
    externalValues.insert(ev);
  }
}

void ReconOSOperator::writeMemory(ValueStorageP vs) {
  if (contains(already_read, vs))
    return;
  already_read.insert(vs);

  if (vs->parent) {
    readMemory(vs->parent);
    const std::string& address_variable = vs->parent->getReadChannel(this)->data_signal;

    std::ostringstream address;
    address << address_variable << " + std_logic_vector(to_unsigned(" << toString(vs->offset) << "*" << (vs->width()/8) << ", 32))";

    writeMemory(address.str(), Channel::make_component_input(calculation, vs->name));
  } else {
    // We cannot write this value because we don't even know its address. Therefore, its address must
    // be passed in with the parameters.
    //TODO At the moment, only pointers and arrays are supported.

    ExternalValue ev = { vs->kind, vs->name, vs };
    externalValues.insert(ev);
  }
}

void ReconOSOperator::addReadParamsState() {
  // The set automatically sorts the variables by kind and name.
  unsigned int address = ram.getNextAddress();
  unsigned int length = 0;
  BOOST_FOREACH(const ExternalValue& ev, externalValues) {
    ChannelP channel = ev.vs->getWriteChannel(this);
    ram.addChannel(channel);
    length += ev.vs->width() / 8;
  }

  BasicReconOSOperator::readMemory("READ_PARAMS", "",
    "addr", "std_logic_vector(to_unsigned(" + toString(length) + ", 24))",
    address,
    1);
}
