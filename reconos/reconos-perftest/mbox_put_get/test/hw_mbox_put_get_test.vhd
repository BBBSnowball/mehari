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

library hwt_mbox_put_get_v1_00_c;
use hwt_mbox_put_get_v1_00_c.hwt_mbox_put_get;

entity hwt_mbox_put_get_test is
end hwt_mbox_put_get_test;

architecture behavior of hwt_mbox_put_get_test is

	component hwt_mbox_put_get is
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

	end component hwt_mbox_put_get;

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

	uut : hwt_mbox_put_get
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
		variable addr_read  : natural;
		variable addr_write : natural;
		variable addr_slv   : osif_word;
		variable result     : osif_word;
	begin
		rst <= '1';
		osif_reset_test(o_osif_test);
		memif_reset_test(o_memif_test);

		wait for 10ns;

		rst <= '0';

		addr_slv := CONV_STD_LOGIC_VECTOR(44, C_OSIF_WIDTH);

		report "mbox_get";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, addr_slv);

		report "mbox_put";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, addr_slv);


		addr_slv := CONV_STD_LOGIC_VECTOR(24, C_OSIF_WIDTH);

		report "mbox_get";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, addr_slv);

		report "mbox_put";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, addr_slv);


		report "Terminating slave thread...";
		-- X"FFFFFFFF" means 'please exit'
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"FFFFFFFF");

		expect_osif_thread_exit(clk, i_osif_test, o_osif_test);
		report "Terminating slave thread... done";

		wait for 100ns;

		endOfSimulation(0);
	end process;
end architecture behavior;
