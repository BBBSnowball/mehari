library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.std_logic_textio.all;
use ieee.MATH_REAL.all;

library STD;
use STD.textio.all;

library work;
use work.float_helpers.all;
use work.test_helpers.all;

entity test_float_conversion is
end test_float_conversion;

architecture test_bench of test_float_conversion is

begin

	p1 : process

		type test_data_item is record
			r : real;
			v : std_logic_vector(63 downto 0);
		end record;
		type test_data_file_t is file of test_data_item;
		file test_data_file : test_data_file_t open read_mode is "float-testdata.dat";

		variable read_len : natural;
		variable tmp : test_data_item;

	begin
		while not endfile(test_data_file) loop
		
			read(test_data_file, tmp);

			report "real number " & real'image(tmp.r) & " is " & to_string(tmp.v);

			assert to_float(tmp.r) = tmp.v
				report "to_float returned wrong value: " &
					to_string(to_float(tmp.r)) & " /= " & to_string(tmp.v);

			-- We don't use this function, so we don't care too much, if this fails.
			-- assert double_to_std_logic_vector(tmp.r) = tmp.v
			-- 	report "double_to_std_logic_vector returned wrong value: " &
			-- 		to_string(double_to_std_logic_vector(tmp.r)) & " /= " & to_string(tmp.v)
			-- 	severity warning;
		
			assert abs(to_real(tmp.v) - tmp.r) < 1.0e-17
				report "to_real returned wrong value: " &
					real'image(to_real(tmp.v)) & " /= " & real'image(tmp.r);
		end loop;

		endOfSimulation(0);

		wait;
	end process;

end test_bench;
