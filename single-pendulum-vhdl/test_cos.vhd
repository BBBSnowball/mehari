--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   12:43:14 12/16/2013
-- Design Name:   
-- Module Name:   /home/benny/Documents/Studium/Masterarbeit/code/abc/test_cos.vhd
-- Project Name:  abc
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: float_cos
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
use ieee.MATH_REAL.all;

library work;
use work.double_type.all;
use work.float_helpers.all;
use work.test_helpers.all;


ENTITY test_cos IS
END test_cos;

ARCHITECTURE behavior OF test_cos IS 
 
  -- Component Declaration for the Unit Under Test (UUT)
 
  COMPONENT float_cos
  PORT(
       aclk : IN  std_logic;
       a_tdata : IN  std_logic_vector(63 downto 0);
       a_tvalid : IN  std_logic;
       a_tready : OUT  std_logic;
       result_tdata : OUT  std_logic_vector(63 downto 0);
       result_tvalid : OUT  std_logic;
       result_tready : IN  std_logic
      );
  END COMPONENT;

  --Inputs
  signal aclk : std_logic := '0';
  signal a_tdata : std_logic_vector(63 downto 0) := (others => '0');
  signal a_tvalid : std_logic := '0';
  signal result_tready : std_logic := '0';

  --Outputs
  signal a_tready : std_logic;
  signal result_tdata : std_logic_vector(63 downto 0);
  signal result_tvalid : std_logic;

  -- Clock period definitions
  constant aclk_period : time := 10 ns;

BEGIN
  -- Instantiate the Unit Under Test (UUT)
  uut: float_cos PORT MAP (
    aclk => aclk,
    a_tdata =>       a_tdata,
    a_tvalid =>      a_tvalid,
    a_tready =>      a_tready,
    result_tdata =>  result_tdata,
    result_tvalid => result_tvalid,
    result_tready => result_tready
  );

  -- Clock process definitions
  aclk_process :process
  begin
    aclk <= '0';
    wait for aclk_period/2;
    aclk <= '1';
    wait for aclk_period/2;
  end process;

  stimulus_process: process
    constant max_clock_cycles : integer := 100;

    procedure test(input_value : in real;
                   epsilon     : in real := 1.0e-10) is
    begin
      wait until a_tready = '1' and rising_edge(aclk) for 10*aclk_period;
      assert a_tready = '1' report "uut is not ready for data: a_tready = " & std_logic'image(a_tready);

      wait until falling_edge(aclk);
      report "testing with " & real'image(input_value) & " (" & to_string(to_float(input_value)) & ")";
      a_tdata <= to_float(input_value);
      a_tvalid <= '1';

      wait for aclk_period;

      a_tvalid <= '0';
      a_tdata <= (others => '0');

      wait until result_tvalid = '1' and rising_edge(aclk) for max_clock_cycles*aclk_period - aclk_period/2;
      assert result_tvalid = '1' report "result was not ready in time";
      wait for 0 ns;

      if result_tvalid = '1' then
        assertAlmostEqual(to_real(result_tdata), cos(input_value), epsilon);
      end if;
    end procedure;
  begin
    wait for 100 ns;

    result_tready <= '1';

    wait for aclk_period*10;

    test(0.0);
    test(math_pi/2.0);
    test(math_pi);
    test(1.2, epsilon => 1.0e-6);
    test(2.0, epsilon => 1.0e-9);

    -- user might want to see what happens after the tests, so we make
    -- sure that the waveform file contains a few more samples
    wait for 1 ns;

    endOfSimulation(0);

    wait;
  end process;

  debug_timing_process : process(aclk)
    variable input_was_valid  : std_logic := '0';
    variable output_was_valid : std_logic := '0';
  begin
    if rising_edge(aclk) then
      if a_tvalid = '1' and input_was_valid = '0' then
        report "data on input is valid";
      end if;

      if result_tvalid = '1' and output_was_valid = '0' then
        report "output data is valid";
      end if;

      input_was_valid := a_tvalid;
      output_was_valid := result_tvalid;
    end if;
  end process;
END;
