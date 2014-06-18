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

library work;
use work.double_type.all;
use work.float_helpers.all;

entity ReturnValueReconOSTest is
end ReturnValueReconOSTest;

architecture behavior of ReturnValueReconOSTest is

	component ReturnValueReconOS is
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
			HWT_Rst   : in  std_logic
		);

	end component ReturnValueReconOS;

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

	uut : ReturnValueReconOS
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
		HWT_Rst                  => HWT_Rst
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
		variable addr_read  : natural;
		variable addr_write : natural;
		variable addr_slv   : osif_word;
		variable result     : osif_word;
		variable a_in       : double;
		variable return_out : double;
	begin
		rst <= '1';
		osif_reset_test(o_osif_test);
		memif_reset_test(o_memif_test);

		wait for 10ns;

		rst <= '0';

		report "Sending address to slave...";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, CONV_STD_LOGIC_VECTOR(42, 32));

		report "Sending data to slave...";
		a_in := to_float(3.7);
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, a_in(31 downto 0));
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, a_in(63 downto 32));

		report "Reading data from slave...";
		return_out := to_float(3.7+2.0);
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, return_out(31 downto 0));
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, return_out(63 downto 32));

		report "Reading 'done' message from slave...";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, CONV_STD_LOGIC_VECTOR(42, 32));

		report "Calculation complete.";


		report "Sending address to slave...";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, CONV_STD_LOGIC_VECTOR(23, 32));

		report "Sending data to slave...";
		a_in := to_float(-1.2);
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, a_in(31 downto 0));
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, a_in(63 downto 32));

		report "Reading data from slave...";
		return_out := to_float(-1.2+2.0);
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, return_out(31 downto 0));
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, return_out(63 downto 32));

		report "Reading 'done' message from slave...";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, CONV_STD_LOGIC_VECTOR(23, 32));

		report "Calculation complete.";


--		report "Sending address to slave...";
--		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, addr_slv);
--
--		report "Sending data to slave...";
--		expect_memif_read(clk, i_memif_test, o_memif_test, addr_read, 8 * C_INPUT_SIZE, input_data);
--
--		report "Reading data from slave...";
--		-- slave only writes outputs
--		expect_memif_write(clk, i_memif_test, o_memif_test, addr_write, 8 * C_OUTPUT_SIZE, output_data, 0, 1000ms);
--		for i in 0 to expected_output_data'length/2-1 loop
--			assertAlmostEqual(
--				to_real(output_data         (output_data'low + 2*i+0) & output_data         (output_data'low + 2*i+1)),
--				to_real(expected_output_data(output_data'low + 2*i+0) & expected_output_data(output_data'low + 2*i+1)));
--		end loop;
--
--		report "Reading 'done' message from slave...";
--		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, addr_slv);
--
--		report "Calculation complete.";
--
--
--		report "Sending 'without_memory' address to slave...";
--		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"FFFFFFFE");
--
--		report "Reading 'done' message from slave...";
--		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"FFFFFFFE");
--
--		report "Calculation complete.";
--
--
--		report "Sending 'set iterations' address to slave...";
--		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"FFFFFFFD");
--
--		report "Sending 'iterations=10' to slave...";
--		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"0000000A");
--
--		report "Sending address to slave...";
--		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, addr_slv);
--
--		for i in 1 to 10 loop
--			report integer'image(i) & ": Sending data to slave...";
--			expect_memif_read(clk, i_memif_test, o_memif_test, addr_read, 8 * C_INPUT_SIZE, input_data);
--
--			report integer'image(i) & ": Reading data from slave...";
--			expect_memif_write(clk, i_memif_test, o_memif_test, addr_write, 8 * C_OUTPUT_SIZE, output_data, 0, 1000ms);
--			for i in 0 to expected_output_data'length/2-1 loop
--				assertAlmostEqual(
--					to_real(output_data         (output_data'low + 2*i+0) & output_data         (output_data'low + 2*i+1)),
--					to_real(expected_output_data(output_data'low + 2*i+0) & expected_output_data(output_data'low + 2*i+1)));
--			end loop;
--		end loop;
--
--		report "Reading 'done' message from slave...";
--		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, addr_slv);
--
--		report "Calculation complete.";


		report "Terminating slave thread...";
		-- X"FFFFFFFF" means 'please exit'
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"FFFFFFFF");

		expect_osif_thread_exit(clk, i_osif_test, o_osif_test);
		report "Terminating slave thread... done";

		wait for 100ns;

		endOfSimulation(0);
	end process;
end architecture behavior;
