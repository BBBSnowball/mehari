LIBRARY ieee;
USE ieee.std_logic_1164.ALL;

package double_type is
  subtype double is std_logic_vector(63 downto 0);
  subtype double_fraction_t is std_logic_vector(51 downto 0);
end package;
