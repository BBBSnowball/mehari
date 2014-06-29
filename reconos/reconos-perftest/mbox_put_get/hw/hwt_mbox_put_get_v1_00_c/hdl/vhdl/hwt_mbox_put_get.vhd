library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

library proc_common_v3_00_a;
use proc_common_v3_00_a.proc_common_pkg.all;

library reconos_v3_01_a;
use reconos_v3_01_a.reconos_pkg.all;

entity hwt_mbox_put_get is
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

	attribute SIGIS   : string;

	attribute SIGIS of HWT_Clk   : signal is "Clk";
	attribute SIGIS of HWT_Rst   : signal is "Rst";

end entity hwt_mbox_put_get;

architecture implementation of hwt_mbox_put_get is
	-- just for simpler use
	signal clk   : std_logic;
	signal rst   : std_logic;

	type STATE_TYPE is (
					STATE_START, STATE_ACK, STATE_PUT_N, STATE_GET_N,
					STATE_POST_N, STATE_WAIT_N, STATE_THREAD_EXIT,
					STATE_GET_READ_ADDRESS, STATE_GET_READ_SIZE, STATE_READ,
					STATE_GET_WRITE_ADDRESS, STATE_GET_WRITE_SIZE, STATE_WRITE);

	constant MBOX_RECV  : std_logic_vector(31 downto 0) := x"00000000";
	constant MBOX_SEND  : std_logic_vector(31 downto 0) := x"00000001";
	constant MBOX_TEST  : std_logic_vector(31 downto 0) := x"00000002";
	constant SEM_TEST   : std_logic_vector(31 downto 0) := x"00000003";

	signal addr, addr2, size : std_logic_vector(31 downto 0);
	signal len          : std_logic_vector(23 downto 0);
	signal state        : STATE_TYPE;
	signal i_osif       : i_osif_t;
	signal o_osif       : o_osif_t;
	signal i_memif      : i_memif_t;
	signal o_memif      : o_memif_t;
	signal i_ram        : i_ram_t;
	signal o_ram        : o_ram_t;
	
	signal o_RAMAddr_reconos_2 : std_logic_vector(0 to 31);
	signal o_RAMData_reconos   : std_logic_vector(0 to 31);
	signal o_RAMWE_reconos     : std_logic;
	signal i_RAMData_reconos   : std_logic_vector(0 to 31);

	signal ignore   : std_logic_vector(31 downto 0);
	signal ignore2  : std_logic_vector(31 downto 0);

	signal count    : std_logic_vector(23 downto 0);
begin

	clk <= HWT_Clk;
	rst <= HWT_Rst;

	-- ReconOS initilization
	osif_setup (
		i_osif,
		o_osif,
		OSIF_FIFO_Sw2Hw_Data,
		OSIF_FIFO_Sw2Hw_Fill,
		OSIF_FIFO_Sw2Hw_Empty,
		OSIF_FIFO_Hw2Sw_Rem,
		OSIF_FIFO_Hw2Sw_Full,
		OSIF_FIFO_Sw2Hw_RE,
		OSIF_FIFO_Hw2Sw_Data,
		OSIF_FIFO_Hw2Sw_WE
	);

	memif_setup (
		i_memif,
		o_memif,
		MEMIF_FIFO_Mem2Hwt_Data,
		MEMIF_FIFO_Mem2Hwt_Fill,
		MEMIF_FIFO_Mem2Hwt_Empty,
		MEMIF_FIFO_Hwt2Mem_Rem,
		MEMIF_FIFO_Hwt2Mem_Full,
		MEMIF_FIFO_Mem2Hwt_RE,
		MEMIF_FIFO_Hwt2Mem_Data,
		MEMIF_FIFO_Hwt2Mem_WE
	);

	ram_setup (
		i_ram,
		o_ram,
		o_RAMAddr_reconos_2,
		o_RAMWE_reconos,
		o_RAMData_reconos,
		i_RAMData_reconos
	);
		
	-- os and memory synchronisation state machine
	reconos_fsm: process (clk,rst,o_osif,o_memif) is
		variable done  : boolean;
	begin
		if rst = '1' then
			osif_reset(o_osif);
			memif_reset(o_memif);
			state <= STATE_START;
			done  := False;
			addr <= (others => '0');
			len <= (others => '0');
		elsif rising_edge(clk) then
			case state is

				-- get task via mbox: the data will be copied from this address to the local ram in the next states
				when STATE_START =>
					osif_mbox_get(i_osif, o_osif, MBOX_RECV, addr, done);
					if done then
						case addr(31 downto 24) is
							when X"FF" =>
								state <= STATE_THREAD_EXIT;
							when X"00" =>
								state <= STATE_ACK;
							when X"01" =>
								state <= STATE_PUT_N;
							when X"02" =>
								state <= STATE_GET_N;
							when X"03" =>
								state <= STATE_POST_N;
							when X"04" =>
								state <= STATE_WAIT_N;
							when X"05" =>
								state <= STATE_GET_READ_ADDRESS;
							when X"06" =>
								state <= STATE_GET_WRITE_ADDRESS;
							when others =>
								state <= STATE_START;
						end case;

						count <= addr(23 downto 0);
					end if;

				when STATE_ACK =>
					osif_mbox_put(i_osif, o_osif, MBOX_SEND, addr, ignore, done);
					if done then state <= STATE_START; end if;

				when STATE_PUT_N =>
					osif_mbox_put(i_osif, o_osif, MBOX_TEST, X"00" & count, ignore, done);
					if done then
						if count /= X"000000" then
							count <= count - X"000001";
						else
							state <= STATE_ACK;
						end if;
					end if;

				when STATE_GET_N =>
					osif_mbox_get(i_osif, o_osif, MBOX_TEST, ignore, done);
					if done then
						if count /= X"000000" then
							count <= count - X"000001";
						else
							state <= STATE_ACK;
						end if;
					end if;

				when STATE_POST_N =>
					osif_sem_post(i_osif, o_osif, SEM_TEST, ignore, done);
					if done then
						if count /= X"000000" then
							count <= count - X"000001";
						else
							state <= STATE_ACK;
						end if;
					end if;

				when STATE_WAIT_N =>
					osif_sem_wait(i_osif, o_osif, SEM_TEST, ignore, done);
					if done then
						if count /= X"000000" then
							count <= count - X"000001";
						else
							state <= STATE_ACK;
						end if;
					end if;

				when STATE_GET_READ_ADDRESS =>
					osif_mbox_get(i_osif, o_osif, MBOX_RECV, addr2, done);
					if done then
						state <= STATE_GET_READ_SIZE;
					end if;

				when STATE_GET_READ_SIZE =>
					osif_mbox_get(i_osif, o_osif, MBOX_RECV, size, done);
					if done then
						state <= STATE_READ;
						len <= size(23 downto 0);
					end if;

				when STATE_READ =>
					memif_read(i_ram,o_ram,i_memif,o_memif,addr2,X"00000000",len,done);
					if done then
						if count /= X"000000" then
							count <= count - X"000001";
						else
							state <= STATE_ACK;
						end if;
					end if;

				when STATE_GET_WRITE_ADDRESS =>
					osif_mbox_get(i_osif, o_osif, MBOX_RECV, addr2, done);
					if done then
						state <= STATE_GET_WRITE_SIZE;
					end if;

				when STATE_GET_WRITE_SIZE =>
					osif_mbox_get(i_osif, o_osif, MBOX_RECV, size, done);
					if done then
						state <= STATE_WRITE;
						len <= size(23 downto 0);
					end if;

				when STATE_WRITE =>
					memif_write(i_ram,o_ram,i_memif,o_memif,X"00000000",addr2,len,done);
					if done then
						if count /= X"000000" then
							count <= count - X"000001";
						else
							state <= STATE_ACK;
						end if;
					end if;

				-- thread exit
				when STATE_THREAD_EXIT =>
					osif_thread_exit(i_osif,o_osif);

			end case;
		end if;
	end process;
	
end architecture;
