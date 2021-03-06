--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   03:22:20 12/21/2013
-- Design Name:   
-- Module Name:   /home/benny/Documents/Studium/Masterarbeit/code/single-pendulum-vhdl/test_float_from_sin.vhd
-- Project Name:  single-pendulum-vhdl
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: float_from_sin
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

ENTITY test_float_from_sin IS
END test_float_from_sin;

ARCHITECTURE behavior OF test_float_from_sin IS 

    -- Component Declaration for the Unit Under Test (UUT)

    COMPONENT float_from_sin
    PORT(
         aclk : IN  std_logic;
         s_axis_a_tvalid : IN  std_logic;
         s_axis_a_tready : OUT  std_logic;
         s_axis_a_tdata : IN  std_logic_vector(47 downto 0);
         m_axis_result_tvalid : OUT  std_logic;
         m_axis_result_tready : IN  std_logic;
         m_axis_result_tdata : OUT  std_logic_vector(63 downto 0)
        );
    END COMPONENT;


   --Inputs
   signal aclk : std_logic := '0';
   signal s_axis_a_tvalid : std_logic := '0';
   signal s_axis_a_tdata : std_logic_vector(47 downto 0) := (others => '0');
   signal m_axis_result_tready : std_logic := '0';

 	--Outputs
   signal s_axis_a_tready : std_logic;
   signal m_axis_result_tvalid : std_logic;
   signal m_axis_result_tdata : std_logic_vector(63 downto 0);

   -- Clock period definitions
   constant aclk_period : time := 10 ns;

BEGIN

	-- Instantiate the Unit Under Test (UUT)
   uut: float_from_sin PORT MAP (
          aclk => aclk,
          s_axis_a_tvalid => s_axis_a_tvalid,
          s_axis_a_tready => s_axis_a_tready,
          s_axis_a_tdata => s_axis_a_tdata,
          m_axis_result_tvalid => m_axis_result_tvalid,
          m_axis_result_tready => m_axis_result_tready,
          m_axis_result_tdata => m_axis_result_tdata
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
    procedure test(output_value : in real) is
    begin
      wait until s_axis_a_tready = '1' for 10*aclk_period;
      assert s_axis_a_tready = '1' report "uut is not ready for data";
      wait for 0 ns;

      s_axis_a_tdata <= std_logic_vector(to_cordic_out(output_value));
      s_axis_a_tvalid <= '1';

      wait for 2*aclk_period;

      s_axis_a_tvalid <= '0';
      wait for 0 ns;
      s_axis_a_tdata <= (others => '0');

      wait until m_axis_result_tvalid = '1' for 100*aclk_period;
      assert m_axis_result_tvalid = '1' report "result was not ready in time";
      wait for 0 ns;

      if m_axis_result_tvalid = '1' then
        assertAlmostEqual(to_real(m_axis_result_tdata), output_value);
      end if;
    end procedure;

    constant null_X_40 : std_logic_vector(39 downto 0) := (others => '0');
  begin   
      -- hold reset state for 100 ns.
      wait for 100 ns;

      m_axis_result_tready <= '1';

      wait for aclk_period*10;

      -- insert stimulus here

      assertEqual(std_logic_vector(to_cordic_out( 1.0)), "01000000" & null_X_40);
      assertEqual(std_logic_vector(to_cordic_out( 1.5)), "01100000" & null_X_40);
      assertEqual(std_logic_vector(to_cordic_out(-1.5)), "10100000" & null_X_40);

      test(1.0);
      test(1.5);
      test(math_pi/2.0);
      test(-math_pi/2.0);

      endOfSimulation(0);

      wait;
  end process;

END;
