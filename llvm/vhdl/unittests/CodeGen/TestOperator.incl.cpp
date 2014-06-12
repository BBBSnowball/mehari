
class TestOperator : public ::Operator {
  ::Operator* uut;
  std::string aclk_period;
public:
  TestOperator(const std::string& name, Operator* uut) {
    setName(name);

    this->uut = uut;

    //TODO This should be a named constant in the generated VHDL code.
    aclk_period = "10 ns";

    // We don't have a clock signal, so we tell the Operator class that
    // it shouldn't try to find one.
    this->setCombinatorial();

    BOOST_FOREACH(::Signal* sig, *uut->getIOList()) {
      switch (sig->type()) {
        case Signal::in:
          this->declare(sig->getName(), sig->width());
          this->inPortMap(uut, sig->getName(), sig->getName());
          break;
        case Signal::out:
          this->outPortMap(uut, sig->getName(), sig->getName());
          break;
        default:
          assert(false);
          break;
      }
    }

    vhdl << this->instance(uut, "uut");

    vhdl << "   -- Clock process definitions\n"
         << "   aclk_process: process\n"
         << "   begin\n"
         << "     aclk <= '0';\n"
         << "     wait for " << aclk_period << "/2;\n"
         << "     aclk <= '1';\n"
         << "     wait for " << aclk_period << "/2;\n"
         << "   end process;\n";
  }

  void stdLibs(std::ostream& o) {
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

  void beginStimulusProcess() {
    vhdl << "   stimulus_proc: process\n";
    vhdl << "   begin\n";
    vhdl << "      -- hold reset state for 100 ns.\n";
    vhdl << "      wait for 100 ns;\n";
  }

  void waitUntilReady(unsigned int max_clock_cycles = 10) {
    std::vector<std::string> ready_signals_in, ready_signals_out;
    BOOST_FOREACH(::Signal* sig, *uut->getIOList()) {
      if ((endsWith(sig->getName(), "_ready") || endsWith(sig->getName(), "_tready"))) {
        switch (sig->type()) {
          case Signal::out:
            ready_signals_out.push_back(sig->getName());
            break;
          case Signal::in:
            ready_signals_in.push_back(sig->getName());
            break;
          default:
            assert(false);
            break;
        }
      }
    }
    assert(!ready_signals_out.empty());

    BOOST_FOREACH(const std::string& sig, ready_signals_in) {
      vhdl << "      " << sig << " <= '1';\n";
    }

    vhdl << "      wait until rising_edge(aclk)\n";
    BOOST_FOREACH(const std::string& sig, ready_signals_out) {
      vhdl << "         and " << sig << " = '1'\n";
    }
    vhdl << "         for " << max_clock_cycles << "*" << aclk_period << ";\n";

    BOOST_FOREACH(const std::string& sig, ready_signals_out) {
      vhdl << "      assert " << sig << " = '1' report \"uut is not ready for data (" << sig << ")\";\n";
    }
  }

  void startDataInput() {
    vhdl << "      wait until falling_edge(aclk);\n";
  }

  void setInput(const std::string& name, const std::string& value) {
    vhdl << "      " << getDataSignalFor(name) << " <= " << value << ";\n";
    vhdl << "      " << getValidSignalFor(name) << " <= '1';\n";
  }

  void setUnsignedIntegerInput(const std::string& name, unsigned int value) {
    std::stringstream s;
    s << "std_logic_vector(to_unsigned(" << value << ", " << getDataSignalWidthFor(name) << "))";
    setInput(name, s.str());
  }

  void setFloatInput(const std::string& name, double value) {
    std::stringstream s;
    s << "to_float(real(" << value << "))";
    setInput(name, s.str());
  }

  void endDataInput() {
    vhdl << "      wait for " << aclk_period << ";\n";

    BOOST_FOREACH(::Signal* sig, *uut->getIOList()) {
      if (!sig->type() == Signal::in)
        continue;

      if (endsWith(sig->getName(), "_tdata") || endsWith(sig->getName(), "_data"))
        vhdl << "      " << sig->getName() << " <= (others => '0');\n";
      else if (endsWith(sig->getName(), "_tvalid") || endsWith(sig->getName(), "_valid"))
        vhdl << "      " << sig->getName() << " <= '0';\n";
    }
  }

  void waitUntilResultIsValid(const std::string& name, unsigned int max_clock_cycles = 100) {
    vhdl << "      wait until " << getValidSignalFor(name) << " = '1' and rising_edge(aclk) ";
    if (max_clock_cycles > 0)
      vhdl << "for " << max_clock_cycles << "*" << aclk_period << " - " << aclk_period << "/2;\n";
    else
      vhdl << "for 0 ns;\n";

    vhdl << "      assert " << getValidSignalFor(name) << " = '1' report \"result was not ready in time (" << name << ")\";\n";
  }

  void waitForAndCheckResult(const std::string& name,
      const std::string& expectedValue, unsigned int max_clock_cycles = 100,
      const std::string& conversion = "$1", const std::string& assertProcedure = "assertEqual") {
    waitUntilResultIsValid(name, max_clock_cycles);
    vhdl << "      if " << getValidSignalFor(name) << " = '1' then\n";
    vhdl << "         " << assertProcedure << "(" << evalPattern(conversion, getDataSignalFor(name)) << ",\n"
         << "            " << expectedValue << ");\n";
    vhdl << "      end if;\n";
  }

  void waitForAndCheckFloatResult(const std::string& name, const std::string& expectedValue,
      unsigned int max_clock_cycles = 100) {
    waitForAndCheckResult(name, expectedValue, max_clock_cycles, "to_real($1)", "assertAlmostEqual");
  }

  void waitForAndCheckUnsignedIntegerResult(const std::string& name, const std::string& expectedValue,
      unsigned int max_clock_cycles = 100) {
    waitForAndCheckResult(name, expectedValue, max_clock_cycles, "to_integer(unsigned($1))");
  }

  void endStimulusProcess() {
    vhdl << "      endOfSimulation(0);\n";
    vhdl << "      wait;\n";
    vhdl << "   end process;\n";
  }

  std::string getDataSignalFor(const std::string& channel) {
    if (hasSignal(channel + "_tdata"))
      return channel + "_tdata";
    else if (hasSignal(channel + "_data"))
      return channel + "_data";
    else {
      assert(false);
      return "";
    }
  }

  std::string getValidSignalFor(const std::string& channel) {
    if (hasSignal(channel + "_tvalid"))
      return channel + "_tvalid";
    else if (hasSignal(channel + "_valid"))
      return channel + "_valid";
    else {
      std::cerr << "ERROR: Cannot find 'valid' signal for channel '" << channel << "'!\n";
      assert(false);
      return "";
    }
  }

private:
  bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
  }

  bool hasSignal(const std::string& name) {
    BOOST_FOREACH(::Signal* sig, *uut->getIOList()) {
      if (sig->getName() == name)
        return true;
    }

    return false;
  }

  std::string evalPattern(const std::string& pattern, const std::string& arg1) {
    size_t pos = pattern.find("$1");
    if (pos != std::string::npos) {
      std::string result(pattern);
      result.replace(pos, 2, arg1);
      return result;
    } else
      return pattern;
  }

  unsigned int getDataSignalWidthFor(const std::string& name) {
    const std::string& signal_name = getDataSignalFor(name);

    BOOST_FOREACH(::Signal* sig, *uut->getIOList()) {
      if (sig->getName() == signal_name)
        return sig->width();
    }

    assert(false);
  }
};
