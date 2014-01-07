LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.math_real.ALL;

library work;
use work.float_helpers.all;
use work.test_helpers.all;

ENTITY test_to_cordic_test_helpers IS
END test_to_cordic_test_helpers;

ARCHITECTURE behavior OF test_to_cordic_test_helpers IS
    constant null_X_40 : std_logic_vector(39 downto 0) := (others => '0');
BEGIN
    blub : process
    begin
        assertEqual(std_logic_vector(to_cordic_in( 2.0)), "01000000" & null_X_40);
        assertEqual(std_logic_vector(to_cordic_in( 1.5)), "00110000" & null_X_40);
        assertEqual(std_logic_vector(to_cordic_in(-1.5)), "11010000" & null_X_40);

        assertEqual(std_logic_vector(to_cordic_out( 1.0)), "01000000" & null_X_40);
        assertEqual(std_logic_vector(to_cordic_out( 1.5)), "01100000" & null_X_40);
        assertEqual(std_logic_vector(to_cordic_out(-1.5)), "10100000" & null_X_40);

        endOfSimulation(0);

        wait;
    end process;
END;
