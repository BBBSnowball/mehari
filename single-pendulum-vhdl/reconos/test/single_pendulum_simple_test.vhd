library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_misc.all;
use ieee.math_real.all;

library reconos_v3_01_a;
use reconos_v3_01_a.reconos_pkg.all;

library reconos_test_v3_01_a;
use reconos_test_v3_01_a.reconos_test_pkg.all;
use reconos_test_v3_01_a.test_helpers.all;

library hwt_single_pendulum_vhdl_v1_00_c;
use hwt_single_pendulum_vhdl_v1_00_c.hwt_single_pendulum_simple;
use hwt_single_pendulum_vhdl_v1_00_c.double_type.all;
use hwt_single_pendulum_vhdl_v1_00_c.float_helpers.all;

entity single_pendulum_simple_test is
end single_pendulum_simple_test;

architecture behavior of single_pendulum_simple_test is

	component hwt_single_pendulum_simple is
		port (
			-- OSIF FIFO ports
			OSIF_FIFO_Sw2Hw_Data    : in  std_logic_vector(31 downto 0);
			OSIF_FIFO_Sw2Hw_Fill    : in  std_logic_vector(15 downto 0);
			OSIF_FIFO_Sw2Hw_Empty   : in  std_logic;
			OSIF_FIFO_Sw2Hw_RE      : out std_logic;

			OSIF_FIFO_Hw2Sw_Data    : out std_logic_vector(31 downto 0);
			OSIF_FIFO_Hw2Sw_Rem     : in  std_logic_vector(15 downto 0);
			OSIF_FIFO_Hw2Sw_Full    : in  std_logic;
			OSIF_FIFO_Hw2Sw_WE      : out std_logic;

			-- MEMIF FIFO ports
			MEMIF_FIFO_Hwt2Mem_Data    : out std_logic_vector(31 downto 0);
			MEMIF_FIFO_Hwt2Mem_Rem     : in  std_logic_vector(15 downto 0);
			MEMIF_FIFO_Hwt2Mem_Full    : in  std_logic;
			MEMIF_FIFO_Hwt2Mem_WE      : out std_logic;

			MEMIF_FIFO_Mem2Hwt_Data    : in  std_logic_vector(31 downto 0);
			MEMIF_FIFO_Mem2Hwt_Fill    : in  std_logic_vector(15 downto 0);
			MEMIF_FIFO_Mem2Hwt_Empty   : in  std_logic;
			MEMIF_FIFO_Mem2Hwt_RE      : out std_logic;

			HWT_Clk   : in  std_logic;
			HWT_Rst   : in  std_logic;

			DEBUG_DATA : out std_logic_vector(5 downto 0)
		);

	end component hwt_single_pendulum_simple;

	-- OSIF FIFO ports
	signal OSIF_FIFO_Sw2Hw_Data     : std_logic_vector(31 downto 0);
	signal OSIF_FIFO_Sw2Hw_Fill     : std_logic_vector(15 downto 0);
	signal OSIF_FIFO_Sw2Hw_Empty    : std_logic;
	signal OSIF_FIFO_Sw2Hw_RE       : std_logic;

	signal OSIF_FIFO_Hw2Sw_Data     : std_logic_vector(31 downto 0);
	signal OSIF_FIFO_Hw2Sw_Rem      : std_logic_vector(15 downto 0);
	signal OSIF_FIFO_Hw2Sw_Full     : std_logic;
	signal OSIF_FIFO_Hw2Sw_WE       : std_logic;

	-- MEMIF FIFO ports
	signal MEMIF_FIFO_Hwt2Mem_Data  : std_logic_vector(31 downto 0);
	signal MEMIF_FIFO_Hwt2Mem_Rem   : std_logic_vector(15 downto 0);
	signal MEMIF_FIFO_Hwt2Mem_Full  : std_logic;
	signal MEMIF_FIFO_Hwt2Mem_WE    : std_logic;

	signal MEMIF_FIFO_Mem2Hwt_Data  : std_logic_vector(31 downto 0);
	signal MEMIF_FIFO_Mem2Hwt_Fill  : std_logic_vector(15 downto 0);
	signal MEMIF_FIFO_Mem2Hwt_Empty : std_logic;
	signal MEMIF_FIFO_Mem2Hwt_RE    : std_logic;

	signal HWT_Clk   : std_logic;
	signal HWT_Rst   : std_logic;

	signal DEBUG_DATA : std_logic_vector(5 downto 0);

	-- some constants from hwt_sort_demo.vhd
	-- The values must be exactly the same as in hwt_sort_demo.vhd !
	constant MBOX_RECV  : std_logic_vector(31 downto 0) := x"00000000";
	constant MBOX_SEND  : std_logic_vector(31 downto 0) := x"00000001";
	constant C_INPUT_SIZE  : integer := 9;	-- 5 params, 1 input, 2 states, and the current time
	constant C_OUTPUT_SIZE : integer := 2;	-- 2 state derivatives
	constant C_LOCAL_RAM_SIZE : integer := 2 * (C_INPUT_SIZE + C_OUTPUT_SIZE);	-- double takes 8 bytes, i.e. 2 words


	signal i_osif_test   : i_osif_test_t;
	signal o_osif_test   : o_osif_test_t;
	signal i_memif_test  : i_memif_test_t;
	signal o_memif_test  : o_memif_test_t;

	constant clk_period : time := 10 ns;
	signal clk : std_logic;
	signal rst  : std_logic;

	type real_array is array(natural range <>) of real;
	constant test_data : real_array(0 to C_INPUT_SIZE+C_OUTPUT_SIZE-1)
		-- see generate_single_pendulum_test_data_output.csv
		--  p0, p1,  p2, p3, p4, u0, x0,      x1,       t,  d0,       d1
		:= (0.2,9.81,0.5,0.2,0.2,0.0,1.998999,-0.222032,0.0,-0.222032,-22.08869242579079);

	shared variable input_data                        : test_memory_t(0 to 2*C_INPUT_SIZE-1);
	shared variable output_data, expected_output_data : test_memory_t(0 to 2*C_OUTPUT_SIZE-1);

	procedure generate_data(constant test_data : in real_array; variable input_data, expected_output_data : out test_memory_t) is
		variable value : double;
	begin
		assert 2 * test_data'length = input_data'length + expected_output_data'length;

		for i in 0 to input_data'length/2-1 loop
			value := to_float(test_data(test_data'low + i));
			input_data(input_data'low + 2*i+0) := value(63 downto 32);
			input_data(input_data'low + 2*i+1) := value(31 downto  0);
		end loop;

		for i in 0 to output_data'length/2-1 loop
			value := to_float(test_data(test_data'low + input_data'length/2 + i));
			output_data(output_data'low + 2*i+0) := value(63 downto 32);
			output_data(output_data'low + 2*i+1) := value(31 downto  0);
		end loop;
	end procedure generate_data;
begin
	-- ReconOS initilization
	osif_setup_test (
		i_osif_test,
		o_osif_test,
		OSIF_FIFO_Sw2Hw_Data,
		OSIF_FIFO_Sw2Hw_Fill,
		OSIF_FIFO_Sw2Hw_Empty,
		OSIF_FIFO_Hw2Sw_Rem,
		OSIF_FIFO_Hw2Sw_Full,
		OSIF_FIFO_Sw2Hw_RE,
		OSIF_FIFO_Hw2Sw_Data,
		OSIF_FIFO_Hw2Sw_WE
	);

	memif_setup_test (
		i_memif_test,
		o_memif_test,
		MEMIF_FIFO_Mem2Hwt_Data,
		MEMIF_FIFO_Mem2Hwt_Fill,
		MEMIF_FIFO_Mem2Hwt_Empty,
		MEMIF_FIFO_Hwt2Mem_Rem,
		MEMIF_FIFO_Hwt2Mem_Full,
		MEMIF_FIFO_Mem2Hwt_RE,
		MEMIF_FIFO_Hwt2Mem_Data,
		MEMIF_FIFO_Hwt2Mem_WE
	);

	uut : hwt_single_pendulum_simple
	port map (
		OSIF_FIFO_Sw2Hw_Data     => OSIF_FIFO_Sw2Hw_Data,
		OSIF_FIFO_Sw2Hw_Fill     => OSIF_FIFO_Sw2Hw_Fill,
		OSIF_FIFO_Sw2Hw_Empty    => OSIF_FIFO_Sw2Hw_Empty,
		OSIF_FIFO_Sw2Hw_RE       => OSIF_FIFO_Sw2Hw_RE,
		OSIF_FIFO_Hw2Sw_Data     => OSIF_FIFO_Hw2Sw_Data,
		OSIF_FIFO_Hw2Sw_Rem      => OSIF_FIFO_Hw2Sw_Rem,
		OSIF_FIFO_Hw2Sw_Full     => OSIF_FIFO_Hw2Sw_Full,
		OSIF_FIFO_Hw2Sw_WE       => OSIF_FIFO_Hw2Sw_WE,
		MEMIF_FIFO_Hwt2Mem_Data  => MEMIF_FIFO_Hwt2Mem_Data,
		MEMIF_FIFO_Hwt2Mem_Rem   => MEMIF_FIFO_Hwt2Mem_Rem,
		MEMIF_FIFO_Hwt2Mem_Full  => MEMIF_FIFO_Hwt2Mem_Full,
		MEMIF_FIFO_Hwt2Mem_WE    => MEMIF_FIFO_Hwt2Mem_WE,
		MEMIF_FIFO_Mem2Hwt_Data  => MEMIF_FIFO_Mem2Hwt_Data,
		MEMIF_FIFO_Mem2Hwt_Fill  => MEMIF_FIFO_Mem2Hwt_Fill,
		MEMIF_FIFO_Mem2Hwt_Empty => MEMIF_FIFO_Mem2Hwt_Empty,
		MEMIF_FIFO_Mem2Hwt_RE    => MEMIF_FIFO_Mem2Hwt_RE,
		HWT_Clk                  => HWT_Clk,
		HWT_Rst                  => HWT_Rst,
		DEBUG_DATA               => DEBUG_DATA
	);

	HWT_Clk <= clk;
	HWT_Rst <= rst;

	-- Clock process definitions
	clk_process : process
	begin
		clk <= '0';
		wait for clk_period/2;
		clk <= '1';
		wait for clk_period/2;
	end process;

	stimulus_process : process is
		variable addr     : natural;
		variable addr_slv : osif_word;
		variable result   : osif_word;
	begin
		report "Generating test data...";
		generate_data(test_data, input_data, expected_output_data);
		report "Generating test data... done";

		rst <= '1';
		osif_reset_test(o_osif_test);
		memif_reset_test(o_memif_test);

		wait for 10ns;

		rst <= '0';

		addr := 44;
		addr_slv := CONV_STD_LOGIC_VECTOR(addr, C_OSIF_WIDTH);

		report "Sending address to slave...";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, addr_slv);

		report "Sending data to slave...";
		expect_memif_read(clk, i_memif_test, o_memif_test, addr, 8 * C_INPUT_SIZE, input_data);

		report "Reading data from slave...";
		-- slave only writes outputs
		addr := addr + 8 * C_INPUT_SIZE;
		expect_memif_write(clk, i_memif_test, o_memif_test, addr, 8 * C_OUTPUT_SIZE, output_data, 0, 1000ms);
		for i in 0 to expected_output_data'length/2-1 loop
			assertAlmostEqual(
				to_real(output_data         (output_data'low + 2*i+0) & output_data         (output_data'low + 2*i+1)),
				to_real(expected_output_data(output_data'low + 2*i+0) & expected_output_data(output_data'low + 2*i+1)));
		end loop;

		report "Reading 'done' message from slave...";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, addr_slv);

		report "Calculation complete.";

		report "Terminating slave thread...";
		-- X"FFFFFFFF" means 'please exit'
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"FFFFFFFF");

		expect_osif_thread_exit(clk, i_osif_test, o_osif_test);
		report "Terminating slave thread... done";

		wait for 100ns;

		endOfSimulation(0);
	end process;
end architecture behavior;
