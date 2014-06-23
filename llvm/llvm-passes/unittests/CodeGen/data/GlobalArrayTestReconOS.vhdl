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

entity GlobalArrayTest is
end GlobalArrayTest;

architecture behavior of GlobalArrayTest is

	component GlobalArrayReconOS is
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

	end component GlobalArrayReconOS;

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

	procedure expect_mbox_get_double(constant mbox : in std_logic_vector(31 downto 0); constant value : real) is
		variable value_as_double : double;
	begin
		value_as_double := to_float(value);
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, mbox, value_as_double(31 downto 0));
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, mbox, value_as_double(63 downto 32));
	end procedure;

	procedure expect_mbox_put_double(constant mbox : in std_logic_vector(31 downto 0); constant value : real) is
		variable value_as_double : double;
	begin
		value_as_double := to_float(value);
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, mbox, value_as_double(31 downto 0));
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, mbox, value_as_double(63 downto 32));
	end procedure;

	function real_value_to_ram(constant value : real) return test_memory_t is
		variable ram : test_memory_t(0 to 1);
		variable value_as_double : double;
	begin
		value_as_double := to_float(value);
		ram(0) := value_as_double(31 downto 0);
		ram(1) := value_as_double(63 downto 32);
		return ram;
	end function;

	procedure real_value_to_ram(
			constant value : real;
			ram : inout test_memory_t;
			constant address : natural
	) is
		variable value_as_double : double;
	begin
		value_as_double := to_float(value);
		ram(address+0) := value_as_double(31 downto 0);
		ram(address+1) := value_as_double(63 downto 32);
	end procedure;

	procedure expect_memif_read_double(constant src_addr : in natural; constant value : real) is
	begin
		expect_memif_read(clk, i_memif_test, o_memif_test, src_addr, 8, real_value_to_ram(value));
	end procedure;

	procedure expect_memif_write_double(constant src_addr : in natural; constant value : real) is
		variable actual : test_memory_t(0 to 1);
	begin
		expect_memif_write(clk, i_memif_test, o_memif_test, src_addr, 8, actual);
		
		assertAlmostEqual(value, to_real(actual(1) & actual(0)));
	end procedure;

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

	uut : GlobalArrayReconOS
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
		variable ram        : test_memory_t(0 to 9);
	begin
		rst <= '1';
		osif_reset_test(o_osif_test);
		memif_reset_test(o_memif_test);

		wait for 10ns;

		rst <= '0';

		report "Sending address to slave...";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, CONV_STD_LOGIC_VECTOR(16, 32));

		--real_value_to_ram(3.7, ram, 0);
		--ram(2) := CONV_STD_LOGIC_VECTOR(44, 32);
		--expect_memif_read(clk, i_memif_test, o_memif_test, 16, 0, ram);

		report "mbox_get for a_in";
		expect_mbox_get_double(MBOX_RECV, 3.7);

		report "writing to x[2]";
		expect_memif_write_double(44+2*8, 3.7);

		report "reading from x[1]";
		expect_memif_read_double(44+1*8, 4.2);

		report "mbox_put for return value";
		expect_mbox_put_double(MBOX_SEND, 3.7+2.0);

		report "Reading 'done' message from slave...";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, CONV_STD_LOGIC_VECTOR(44, 32));

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
