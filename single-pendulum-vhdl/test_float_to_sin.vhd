--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   16:20:25 12/20/2013
-- Design Name:   
-- Module Name:   /home/benny/Documents/Studium/Masterarbeit/code/single-pendulum-vhdl/test_float_to_sin.vhd
-- Project Name:  single-pendulum-vhdl
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: float_to_sin
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

ENTITY test_float_to_sin IS
END test_float_to_sin;
 
ARCHITECTURE behavior OF test_float_to_sin IS 
 
  -- Component Declaration for the Unit Under Test (UUT)

  COMPONENT float_to_sin
  PORT(
       aclk : IN  std_logic;
       s_axis_a_tvalid : IN  std_logic;
       s_axis_a_tready : OUT  std_logic;
       s_axis_a_tdata : IN  std_logic_vector(63 downto 0);
       m_axis_result_tvalid : OUT  std_logic;
       m_axis_result_tready : IN  std_logic;
       m_axis_result_tdata : OUT  std_logic_vector(47 downto 0)
      );
  END COMPONENT;


  --Inputs
  signal aclk : std_logic := '0';
  signal s_axis_a_tvalid : std_logic := '0';
  signal s_axis_a_tdata : std_logic_vector(63 downto 0) := (others => '0');
  signal m_axis_result_tready : std_logic := '0';

  --Outputs
  signal s_axis_a_tready : std_logic;
  signal m_axis_result_tvalid : std_logic;
  signal m_axis_result_tdata : std_logic_vector(47 downto 0);

  -- Clock period definitions
  constant aclk_period : time := 10 ns;

BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
  uut: float_to_sin PORT MAP (
          aclk => aclk,
          s_axis_a_tvalid => s_axis_a_tvalid,
          s_axis_a_tready => s_axis_a_tready,
          s_axis_a_tdata => s_axis_a_tdata,
          m_axis_result_tvalid => m_axis_result_tvalid,
          m_axis_result_tready => m_axis_result_tready,
          m_axis_result_tdata => m_axis_result_tdata
        );

  -- Clock process definitions
  aclk_process: process
  begin
		aclk <= '0';
		wait for aclk_period/2;
		aclk <= '1';
		wait for aclk_period/2;
  end process;
 

  stimulus_process: process
    constant max_clock_cycles : integer := 7;

    procedure test(input_value : in real) is
    begin
      wait until s_axis_a_tready = '1' and rising_edge(aclk) for 10*aclk_period;
      assert s_axis_a_tready = '1' report "uut is not ready for data";

      wait until falling_edge(aclk);
      s_axis_a_tdata <= to_float(input_value);
      s_axis_a_tvalid <= '1';

      wait for aclk_period;

      s_axis_a_tvalid <= '0';
      s_axis_a_tdata <= (others => '0');

      wait until m_axis_result_tvalid = '1' and rising_edge(aclk) for max_clock_cycles*aclk_period - aclk_period/2;
      assert m_axis_result_tvalid = '1' report "result was not ready in time";

      if m_axis_result_tvalid = '1' then
        assertEqual(m_axis_result_tdata, std_logic_vector(to_cordic_in(input_value)));
      end if;
    end procedure;
  begin
      wait for 100 ns;

      m_axis_result_tready <= '1';

      wait for aclk_period*10;

      test(2.0);
      test(1.5);
      test(math_pi);
      test(-math_pi);

      endOfSimulation(0);

      wait;
  end process;

  debug_timing_process : process(aclk)
    variable input_was_valid  : std_logic := '0';
    variable output_was_valid : std_logic := '0';
  begin
    if rising_edge(aclk) then
      if s_axis_a_tvalid = '1' and input_was_valid = '0' then
        report "data on input is valid";
      end if;

      if m_axis_result_tvalid = '1' and output_was_valid = '0' then
        report "output data is valid";
      end if;

      input_was_valid := s_axis_a_tvalid;
      output_was_valid := m_axis_result_tvalid;
    end if;
  end process;
END;
