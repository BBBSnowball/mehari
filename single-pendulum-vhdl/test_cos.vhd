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
  --TODO We shouldn't have to initialize a_tready, but if we don't do this, it will have
  --     the value 'U', if we start the simulation from the command-line. For some reason,
  --     it works in ISE.
  signal a_tready : std_logic := '1';
  signal result_tdata : std_logic_vector(63 downto 0);
  signal result_tvalid : std_logic;

  -- Clock period definitions
  constant aclk_period : time := 10 ns;

  signal dbg_a_tdata, dbg_result_tdata : real;

BEGIN
  dbg_a_tdata <= to_real(a_tdata);
  dbg_result_tdata <= to_real(result_tdata);
 
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

  -- Stimulus process
  stim_proc: process
    procedure test(input_value : in real;
                      expected_output_value : in real;
                      epsilon : in real := 1.0e-10) is
    begin
      wait until a_tready = '1' for 10*aclk_period;
      assert a_tready = '1' report "uut is not ready for data";
      wait for 0 ns;

      report "testing with " & real'image(input_value) & " (" & to_string(to_float(input_value)) & ")";
      a_tdata <= to_float(input_value);
      a_tvalid <= '1';

      wait for 2*aclk_period;

      a_tvalid <= '0';
      wait for 0 ns;
      a_tdata <= (others => '0');

      wait until result_tvalid = '1' for 100*aclk_period;
      assert result_tvalid = '1' report "result was not ready in time";
      wait for 0 ns;

      if result_tvalid = '1' then
        assertAlmostEqual(to_real(result_tdata), cos(input_value), epsilon);
      end if;
    end procedure;
  begin    
    -- hold reset state for 100 ns.
    wait for 100 ns;

    result_tready <= '1';

    wait for aclk_period*10;


    assert a_tready = '1'
      report "uut is not ready (a_tready = " & std_logic'image(a_tready) & ")";


    test(0.0, 0.0        );
    test(math_pi/2.0, 1.0);
    test(math_pi,     0.0);
    test(1.2, 0.932039, epsilon => 1.0e-6);

    wait for 1 ns;

    endOfSimulation(0);

    wait;
  end process;

  --debug_process: process
  --begin
  -- report("y: " & real'image(y));
  -- report("y_e: " & integer'image(y_e));
  -- report("y_frac: " & real'image(y_frac));
  -- report("y_fraction: " & to_string(y_fraction));
  -- report("y_d: " & to_string(y_d));
  -- wait for 1000ms;
  --end process;
END;
