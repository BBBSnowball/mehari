----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    11:37:52 12/16/2013 
-- Design Name: 
-- Module Name:    float_sin - Structural 
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

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity sin is
	Port( 
      aclk : IN STD_LOGIC;
      arg0_data : IN STD_LOGIC_VECTOR (63 downto 0);
			arg0_valid : IN STD_LOGIC;
			arg0_ready : OUT STD_LOGIC;
			
			result_data : OUT STD_LOGIC_VECTOR (63 downto 0);
			result_valid : OUT STD_LOGIC;
			result_ready : IN STD_LOGIC );
end sin;

architecture Structural of sin is

  component float_sin is
    Port( 
        aclk : IN STD_LOGIC;
        a_tdata : IN STD_LOGIC_VECTOR (63 downto 0);
        a_tvalid : IN STD_LOGIC;
        a_tready : OUT STD_LOGIC;
        
        result_tdata : OUT STD_LOGIC_VECTOR (63 downto 0);
        result_tvalid : OUT STD_LOGIC;
        result_tready : IN STD_LOGIC );
  end component;

begin

t1 : float_sin
  PORT MAP (
    aclk => aclk,
    a_tvalid => arg0_valid,
    a_tready => arg0_ready,
    a_tdata  => arg0_data,
    result_tdata  => result_data,
    result_tvalid => result_valid,
    result_tready => result_ready
  );

end Structural;

