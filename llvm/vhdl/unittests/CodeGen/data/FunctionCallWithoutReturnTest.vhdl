-- generated by Mehari
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library work;

entity test is
   port ( 
         aclk : in std_logic;
         a_in_data : in  std_logic_vector(31 downto 0);
         a_in_valid : in std_logic;
         a_in_ready : out std_logic
   );
end entity;

architecture arch of test is
   component func is
      port ( 
         aclk : in std_logic;
         arg0_data : in  std_logic_vector(31 downto 0);
         arg0_valid : in std_logic;
         arg0_ready : out std_logic;
         arg1_data : in  std_logic_vector(31 downto 0);
         arg1_valid : in std_logic;
         arg1_ready : out std_logic;
         done : out std_logic
   );
   end component;

signal a_in_ready_ : std_logic;
begin
   a_in_ready <= a_in_ready_;
   inst0: func
      port map ( aclk => aclk,
                 arg0_data => a_in_data,
                 arg0_ready => a_in_ready_,
                 arg0_valid => a_in_valid,
                 arg1_data => 2,
                 arg1_valid => '1');
end architecture;

