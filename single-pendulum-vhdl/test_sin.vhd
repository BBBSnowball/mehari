--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   12:43:14 12/16/2013
-- Design Name:   
-- Module Name:   /home/benny/Documents/Studium/Masterarbeit/code/abc/test_sin.vhd
-- Project Name:  abc
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: float_sin
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

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
USE ieee.numeric_std.ALL;

use ieee.MATH_REAL.all;

library work;
use work.double_type.all;
use work.float_helpers.all;


ENTITY test_sin IS
END test_sin;

ARCHITECTURE behavior OF test_sin IS 
 
  -- Component Declaration for the Unit Under Test (UUT)
 
  COMPONENT float_sin
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
  signal a_tready : std_logic; -- := '0';
  signal result_tdata : std_logic_vector(63 downto 0);
  signal result_tvalid : std_logic;

  type float_sin_signals is
    record
      a_tdata : double;
      a_tvalid, a_tready : std_logic;

      result_tdata : double;
      result_tvalid, result_tready : std_logic;
    end record;

  signal uut_signals : float_sin_signals := ((others => '0'), '0', 'U', (others => 'U'), 'U', '0');

  -- Clock period definitions
  constant aclk_period : time := 10 ns;

  procedure assertAlmostEqual(actual : in real;
                              expected : in real;
                              epsilon : in real := 1.0e-10) is    --TODO is this a good default value?
  begin
    assert abs(actual - expected) < epsilon
      report real'image(actual) & " /= " & real'image(expected)
        & ", difference is " & real'image(actual-expected);
  end procedure;

  procedure testSin(input_value : in real;
                    expected_output_value : in real;
                    signal a_tdata : out double;
                    signal a_tvalid : out std_logic;
                    signal a_tready : in std_logic;
                    signal result_tdata : in double;
                    signal result_tvalid : in std_logic;
                    signal result_tready : out std_logic;
                    epsilon : in real := 1.0e-10) is
  begin
    a_tdata <= to_float(input_value);
    a_tvalid <= '1';

    wait for 2*aclk_period;

    a_tvalid <= '0';
    a_tdata <= (others => '0');

    wait until result_tvalid = '1' for 100*aclk_period;
    assert result_tvalid = '1' report "result was not ready in time";

    if result_tvalid = '1' then
      assertAlmostEqual(to_real(result_tdata), expected_output_value, epsilon);
    end if;
  end procedure;

  procedure endOfSimulation(dummy : in integer := 0) is
  begin
    -- http://www.velocityreviews.com/forums/t57165-how-to-stop-simulation-in-vhdl.html
    assert false report "NONE. End of simulation." severity failure;
  end;

BEGIN
 
  -- Instantiate the Unit Under Test (UUT)
  uut: float_sin PORT MAP (
    aclk => aclk,
    a_tdata => a_tdata,
    a_tvalid => a_tvalid,
    a_tready => a_tready,
    result_tdata => result_tdata,
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
  begin    
    -- hold reset state for 100 ns.
    wait for 100 ns;  

    wait for aclk_period*10;


    assert a_tready = '1' report "uut is not ready (a_tready = " & std_logic'image(a_tready) & ")";

    result_tready <= '1';


    testSin(0.0, 0.0, a_tdata, a_tvalid, a_tready, result_tdata, result_tvalid, result_tready);
    testSin(math_pi/2.0, 1.0, a_tdata, a_tvalid, a_tready, result_tdata, result_tvalid, result_tready);
    testSin(math_pi,     0.0, a_tdata, a_tvalid, a_tready, result_tdata, result_tvalid, result_tready);
    testSin(1.2, 0.932039, a_tdata, a_tvalid, a_tready, result_tdata, result_tvalid, result_tready, epsilon => 1.0e-6);

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
