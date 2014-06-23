-- generated by Mehari
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.math_real.ALL;
use ieee.numeric_std.all;

library work;
use work.float_helpers.all;
use work.test_helpers.all;

entity test is
   port ( 
         aclk : in std_logic;
         reset : in std_logic;
         a_in_data : in  std_logic_vector(63 downto 0);
         a_in_valid : in std_logic;
         a_in_ready : out std_logic;
         a_mbox_data : out  std_logic_vector(63 downto 0);
         a_mbox_valid : out std_logic;
         a_mbox_ready : in std_logic;
         t0_mbox_data : in  std_logic_vector(63 downto 0);
         t0_mbox_valid : in std_logic;
         t0_mbox_ready : out std_logic;
         a_out_data : out  std_logic_vector(63 downto 0);
         a_out_valid : out std_logic;
         a_out_ready : in std_logic
   );
end entity;

architecture arch of test is
signal t0_data :  std_logic_vector(63 downto 0);
signal t0_valid : std_logic;
signal t0_ready : std_logic;
begin
   a_mbox_data <= a_in_data;
   a_mbox_valid <= a_in_valid;
   a_in_ready <= a_mbox_ready;
   t0_data <= t0_mbox_data;
   t0_valid <= t0_mbox_valid;
   t0_mbox_ready <= t0_ready;
   a_out_data <= t0_data;
   a_out_valid <= t0_valid;
   t0_ready <= a_out_ready;
end architecture;

