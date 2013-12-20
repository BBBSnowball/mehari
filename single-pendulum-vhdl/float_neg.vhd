----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    11:14:03 12/16/2013 
-- Design Name: 
-- Module Name:    float_neg - Behavioral 
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

entity float_neg is
    Port ( a_tvalid : in  STD_LOGIC;
           a_tready : out  STD_LOGIC;
           a_tdata : in  STD_LOGIC_VECTOR (63 downto 0);
           result_tvalid : out  STD_LOGIC;
           result_tready : in  STD_LOGIC;
           result_tdata : out  STD_LOGIC_VECTOR (63 downto 0));
end float_neg;

architecture Behavioral of float_neg is

begin

	result_tvalid <= a_tvalid;
	a_tready <= result_tready;
	result_tdata <= "1" & a_tdata(62 downto 0);

end Behavioral;

