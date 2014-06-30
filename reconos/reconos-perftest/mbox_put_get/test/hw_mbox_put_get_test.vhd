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

	shared variable ram : test_memory_t(0 to 20);

	-- some constants from hwt_sort_demo.vhd
	-- The values must be exactly the same as in hwt_sort_demo.vhd !
	constant MBOX_RECV  : std_logic_vector(31 downto 0) := x"00000000";
	constant MBOX_SEND  : std_logic_vector(31 downto 0) := x"00000001";
	constant MBOX_TEST  : std_logic_vector(31 downto 0) := x"00000002";
	constant SEM_TEST   : std_logic_vector(31 downto 0) := x"00000003";

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
	begin
		rst <= '1';
		osif_reset_test(o_osif_test);
		memif_reset_test(o_memif_test);

		wait for 10ns;

		rst <= '0';


		report "start: noop";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"00000042");

		report "ack:   noop";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"00000042");


		report "start: noop";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"000000AA");

		report "ack:   noop";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"000000AA");


		report "start: 5x mbox_put";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"01000004");

		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000004");
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000003");
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000002");
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000001");
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000000");

		report "ack:   5x mbox_put";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"01000004");


		report "start: 6x mbox_get";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"02000005");

		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000042");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000042");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000042");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000042");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000042");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_TEST, X"00000042");

		report "ack:   6x mbox_get";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"02000005");


		report "start: 7x sem_post";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"03000006");

		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_post(clk, i_osif_test, o_osif_test, SEM_TEST);

		report "ack:   7x sem_post";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"03000006");


		report "start: 8x sem_wait";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"04000007");

		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);
		expect_osif_sem_wait(clk, i_osif_test, o_osif_test, SEM_TEST);

		report "ack:   8x sem_wait";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"04000007");


		report "start: 2x memory read";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"05000001");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"00000010");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"00000008");

		report "abc";
		expect_memif_read(clk, i_memif_test, o_memif_test, 16, 8, ram);
		report "abc";
		expect_memif_read(clk, i_memif_test, o_memif_test, 16, 8, ram);

		report "ack:   2x memory read";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"05000001");


		report "start: 3x memory write";
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"06000002");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"00000010");
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"00000008");

		expect_memif_write(clk, i_memif_test, o_memif_test, 16, 8, ram);
		expect_memif_write(clk, i_memif_test, o_memif_test, 16, 8, ram);
		expect_memif_write(clk, i_memif_test, o_memif_test, 16, 8, ram);

		report "ack:   3x memory write";
		expect_osif_mbox_put(clk, i_osif_test, o_osif_test, MBOX_SEND, X"06000002");


		report "Terminating slave thread...";
		-- X"FFFFFFFF" means 'please exit'
		expect_osif_mbox_get(clk, i_osif_test, o_osif_test, MBOX_RECV, X"FFFFFFFF");

		expect_osif_thread_exit(clk, i_osif_test, o_osif_test);
		report "Terminating slave thread... done";

		wait for 100ns;

		endOfSimulation(0);
	end process;
end architecture behavior;
