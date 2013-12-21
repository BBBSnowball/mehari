----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    11:37:52 12/16/2013 
-- Design Name: 
-- Module Name:    float_cos - Structural 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity float_cos is
	Port( 
      aclk : IN STD_LOGIC;
      a_tdata : IN STD_LOGIC_VECTOR (63 downto 0);
			a_tvalid : IN STD_LOGIC;
			a_tready : OUT STD_LOGIC;
			
			result_tdata : OUT STD_LOGIC_VECTOR (63 downto 0);
			result_tvalid : OUT STD_LOGIC;
			result_tready : IN STD_LOGIC );
end float_cos;

architecture Structural of float_cos is

  COMPONENT float_to_sin IS
    PORT (
      aclk : IN STD_LOGIC;
      s_axis_a_tvalid : IN STD_LOGIC;
      s_axis_a_tready : OUT STD_LOGIC;
      s_axis_a_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
      m_axis_result_tvalid : OUT STD_LOGIC;
      m_axis_result_tready : IN STD_LOGIC;
      m_axis_result_tdata : OUT STD_LOGIC_VECTOR(47 DOWNTO 0)
    );
  END COMPONENT;

  COMPONENT cordic_sin IS
    PORT (
      aclk : IN STD_LOGIC;
      s_axis_phase_tvalid : IN STD_LOGIC;
      s_axis_phase_tready : OUT STD_LOGIC;
      s_axis_phase_tdata : IN STD_LOGIC_VECTOR(47 DOWNTO 0);
      m_axis_dout_tvalid : OUT STD_LOGIC;
      m_axis_dout_tready : IN STD_LOGIC;
      m_axis_dout_tdata : OUT STD_LOGIC_VECTOR(95 DOWNTO 0)
    );
  END COMPONENT;

  COMPONENT float_from_sin IS
    PORT (
      aclk : IN STD_LOGIC;
      s_axis_a_tvalid : IN STD_LOGIC;
      s_axis_a_tready : OUT STD_LOGIC;
      s_axis_a_tdata : IN STD_LOGIC_VECTOR(47 DOWNTO 0);
      m_axis_result_tvalid : OUT STD_LOGIC;
      m_axis_result_tready : IN STD_LOGIC;
      m_axis_result_tdata : OUT STD_LOGIC_VECTOR(63 DOWNTO 0)
    );
  END COMPONENT;

  signal temp1 : STD_LOGIC_VECTOR(47 downto 0);
  signal temp2 : STD_LOGIC_VECTOR(95 downto 0);
  signal temp1_valid, temp2_valid : STD_LOGIC;
  signal temp1_ready, temp2_ready : STD_LOGIC;

begin

t1 : float_to_sin
  PORT MAP (
    aclk => aclk,
    s_axis_a_tvalid => a_tvalid,
    s_axis_a_tready => a_tready,
    s_axis_a_tdata  => a_tdata,
    m_axis_result_tvalid => temp1_valid,
    m_axis_result_tready => temp1_ready,
    m_axis_result_tdata => temp1
  );

sin : cordic_sin
  PORT MAP (
    aclk => aclk,
    s_axis_phase_tvalid => temp1_valid,
    s_axis_phase_tready => temp1_ready,
    s_axis_phase_tdata => temp1,
    m_axis_dout_tvalid => temp2_valid,
    m_axis_dout_tready => temp2_ready,
    m_axis_dout_tdata => temp2
  );

t2 : float_from_sin
  PORT MAP (
    aclk => aclk,
    s_axis_a_tvalid => temp2_valid,
    s_axis_a_tready => temp2_ready,
    s_axis_a_tdata => temp2(47 downto 0),
    m_axis_result_tvalid => result_tvalid,
    m_axis_result_tready => result_tready,
    m_axis_result_tdata =>  result_tdata
  );

end Structural;
