--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   02:13:07 12/21/2013
-- Design Name:   
-- Module Name:   /home/benny/Documents/Studium/Masterarbeit/code/single-pendulum-vhdl/test_cordic_sin.vhd
-- Project Name:  single-pendulum-vhdl
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: cordic_sin
-- 
-- Dependencies:
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
--
-- Notes: 
-- This testbench has been automatically generated using types std_logic and
-- std_logic_vector for the ports of the unit under test.  Xilinx recommends
-- that these types always be used for the top-level I/O of a design in order
-- to guarantee that the testbench will bind correctly to the post-implementation 
-- simulation model.
--------------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.math_real.ALL;

library work;
use work.float_helpers.all;
use work.test_helpers.all;

ENTITY test_cordic_sin IS
END test_cordic_sin;

ARCHITECTURE behavior OF test_cordic_sin IS
  -- For some reason from_fixedpoint is VERY slow, so we avoid using it. However,
  -- debug output is much better, if we do use it. At the moment, we have to use
  -- from_fixedpoint because with the other method we do not compensate the numerical
  -- imprecision, which makes the tests fail.
  constant use_from_fixedpoint : boolean := false;

  -- Component Declaration for the Unit Under Test (UUT)

  COMPONENT cordic_sin
  PORT(
       aclk : IN  std_logic;
       s_axis_phase_tvalid : IN  std_logic;
       s_axis_phase_tready : OUT  std_logic;
       s_axis_phase_tdata : IN  std_logic_vector(47 downto 0);
       m_axis_dout_tvalid : OUT  std_logic;
       m_axis_dout_tready : IN  std_logic;
       m_axis_dout_tdata : OUT  std_logic_vector(95 downto 0)
      );
  END COMPONENT;


  --Inputs
  signal aclk : std_logic := '0';
  signal s_axis_phase_tvalid : std_logic := '0';
  signal s_axis_phase_tdata : std_logic_vector(47 downto 0) := (others => '0');
  signal m_axis_dout_tready : std_logic := '0';

  --Outputs
  signal s_axis_phase_tready : std_logic;
  signal m_axis_dout_tvalid : std_logic;
  signal m_axis_dout_tdata : std_logic_vector(95 downto 0);

  -- Clock period definitions
  constant aclk_period : time := 10 ns;
 
BEGIN

  -- Instantiate the Unit Under Test (UUT)
  uut: cordic_sin PORT MAP (
        aclk => aclk,
        s_axis_phase_tvalid => s_axis_phase_tvalid,
        s_axis_phase_tready => s_axis_phase_tready,
        s_axis_phase_tdata => s_axis_phase_tdata,
        m_axis_dout_tvalid => m_axis_dout_tvalid,
        m_axis_dout_tready => m_axis_dout_tready,
        m_axis_dout_tdata => m_axis_dout_tdata
      );

  -- Clock process definitions
  aclk_process: process
  begin
    aclk <= '0';
    wait for aclk_period/2;
    aclk <= '1';
    wait for aclk_period/2;
  end process;


  -- Stimulus process
  stim_proc: process
    procedure test(input_value : in real) is
      variable expected_output : real;
      variable actual, expected : std_logic_vector(47 downto 0);
      constant expected_precision : integer := 24;  -- leading bits that must be equal
      variable actual_for_comparison, expected_for_comparison
        : std_logic_vector(actual'high downto actual'high-expected_precision);
    begin
      wait until s_axis_phase_tready = '1' for 10*aclk_period;
      assert s_axis_phase_tready = '1' report "uut is not ready for data";
      wait for 0 ns;

      s_axis_phase_tdata <= std_logic_vector(to_cordic_in(input_value));
      s_axis_phase_tvalid <= '1';

      wait for 2*aclk_period;

      s_axis_phase_tvalid <= '0';
      s_axis_phase_tdata <= (others => '0');

      wait until m_axis_dout_tvalid = '1' for 100*aclk_period;
      assert m_axis_dout_tvalid = '1' report "result was not ready in time";
      wait for 0 ns;

      if m_axis_dout_tvalid = '1' then
        expected_output := sin(input_value);
        if use_from_fixedpoint then
          assertAlmostEqual(from_fixedpoint(fixedpoint(m_axis_dout_tdata(47 downto 0))), expected_output);
        else
          -- Only the first half or so is as expected due to
          -- lack of numerical precision. If the value is near zero, the leading bits
          -- may differ although the difference in the represented value is small.
          actual := m_axis_dout_tdata(47 downto 0);
          expected := std_logic_vector(to_cordic_out(expected_output));
          actual_for_comparison := actual(actual_for_comparison'range);
          expected_for_comparison := expected(expected_for_comparison'range);
          if not actual_for_comparison = expected_for_comparison and not (
              (actual_for_comparison = (actual_for_comparison'range => '1') and expected_for_comparison = (expected_for_comparison'range => '0')) or
              (actual_for_comparison = (actual_for_comparison'range => '0') and expected_for_comparison = (expected_for_comparison'range => '1'))) then
            assertEqual(actual_for_comparison, expected_for_comparison);
          end if;
        end if;
      end if;
    end procedure;

    constant null_X_40 : std_logic_vector(39 downto 0) := (others => '0');
  begin		
    -- hold reset state for 100 ns.
    wait for 100 ns;

    m_axis_dout_tready <= '1';

    wait for aclk_period*10;

    if use_from_fixedpoint then
      assertAlmostEqual(from_fixedpoint(cordic_in_t("01000000" & null_X_40)),  2.0);
      assertAlmostEqual(from_fixedpoint(cordic_in_t("00110000" & null_X_40)),  1.5);
      assertAlmostEqual(from_fixedpoint(cordic_in_t("11010000" & null_X_40)), -1.5);
    else
      report "from_fixedpoint is not tested";
    end if;

    test(2.0);
    test(1.5);
    test(math_pi);
    test(-math_pi);

    endOfSimulation(0);

    wait;
  end process;

END;
