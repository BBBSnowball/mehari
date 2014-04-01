library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

library proc_common_v3_00_a;
use proc_common_v3_00_a.proc_common_pkg.all;

library reconos_v3_01_a;
use reconos_v3_01_a.reconos_pkg.all;

entity hwt_single_pendulum_simple is
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

end entity hwt_single_pendulum_simple;

architecture implementation of hwt_single_pendulum_simple is
	-- just for simpler use
	signal clk   : std_logic;
	signal rst   : std_logic;

	type STATE_TYPE is (
					STATE_GET_ADDR,STATE_READ,STATE_STARTING,STATE_EVAL,
					STATE_WRITE,STATE_ACK,STATE_THREAD_EXIT);

	component single_pendulum is
	Port ( x0, x1, u0, t  : in  STD_LOGIC_VECTOR(63 downto 0);
		   p0, p1, p2, p4 : in  STD_LOGIC_VECTOR(63 downto 0);
		   dx0, dx1       : out STD_LOGIC_VECTOR(63 downto 0);
		   in_tvalid      : in  STD_LOGIC;
		   in_tready      : out STD_LOGIC;
		   out_tvalid     : out STD_LOGIC;
		   out_tready     : in  STD_LOGIC;
		   aclk           : in  STD_LOGIC );
	end component;

	constant C_INPUT_SIZE  : integer := 9;	-- 5 params, 1 input, 2 states, and the current time
	constant C_OUTPUT_SIZE : integer := 2;	-- 2 state derivatives
	constant C_LOCAL_RAM_SIZE          : integer := 2 * (C_INPUT_SIZE + C_OUTPUT_SIZE);	-- double takes 8 bytes, i.e. 2 words
	constant C_LOCAL_RAM_ADDRESS_WIDTH : integer := clog2(C_LOCAL_RAM_SIZE);
	constant C_LOCAL_RAM_SIZE_IN_BYTES : integer := 4*C_LOCAL_RAM_SIZE;

	constant MBOX_RECV  : std_logic_vector(31 downto 0) := x"00000000";
	constant MBOX_SEND  : std_logic_vector(31 downto 0) := x"00000001";

	signal addr         : std_logic_vector(31 downto 0);
	signal addr_outputs : std_logic_vector(31 downto 0);
	signal len          : std_logic_vector(23 downto 0);
	signal state        : STATE_TYPE;
	signal i_osif       : i_osif_t;
	signal o_osif       : o_osif_t;
	signal i_memif      : i_memif_t;
	signal o_memif      : o_memif_t;
	signal i_ram        : i_ram_t;
	signal o_ram        : o_ram_t;

	signal o_RAMAddr_reconos   : std_logic_vector(0 to C_LOCAL_RAM_ADDRESS_WIDTH-1);
	signal o_RAMAddr_reconos_2 : std_logic_vector(0 to 31);
	signal o_RAMData_reconos   : std_logic_vector(0 to 31);
	signal o_RAMWE_reconos     : std_logic;
	signal i_RAMData_reconos   : std_logic_vector(0 to 31);
	
	signal ignore   : std_logic_vector(31 downto 0);

	signal sort_start : std_logic := '0';
	signal sort_done  : std_logic := '0';

	-- signal to/from single_pendulum
	signal x0, x1, u0, t  : STD_LOGIC_VECTOR(63 downto 0);
	signal p0, p1, p2, p4 : STD_LOGIC_VECTOR(63 downto 0);
	signal dx0, dx1       : STD_LOGIC_VECTOR(63 downto 0);
	signal in_tvalid      : STD_LOGIC;
	signal in_tready      : STD_LOGIC;
	signal out_tvalid     : STD_LOGIC;
	signal out_tready     : STD_LOGIC;
begin

	DEBUG_DATA(5) <= '1' when state = STATE_GET_ADDR else '0';
	DEBUG_DATA(4) <= '1' when state = STATE_READ else '0';
	DEBUG_DATA(3) <= '1' when state = STATE_EVAL else '0';
	DEBUG_DATA(2) <= '1' when state = STATE_WRITE else '0';
	DEBUG_DATA(1) <= '1' when state = STATE_ACK else '0';
	DEBUG_DATA(0) <= '1' when state = STATE_THREAD_EXIT else '0';

	clk <= HWT_Clk;
	rst <= HWT_Rst;
	
	-- dummy RAM writes values to single_pendulum
	local_ram_ctrl_1 : process (clk) is
	begin
		if (rising_edge(clk)) then
			if (o_RAMWE_reconos = '1') then
				-- RAM contents:
				-- 5 params, 1 input, 2 states, and the current time
				-- 2 state derivatives
				case conv_integer(o_RAMAddr_reconos) is
					when  0 => p0(63 downto 32) <= o_RAMData_reconos;
					when  1 => p0(31 downto  0) <= o_RAMData_reconos;
					when  2 => p1(63 downto 32) <= o_RAMData_reconos;
					when  3 => p1(31 downto  0) <= o_RAMData_reconos;
					when  4 => p2(63 downto 32) <= o_RAMData_reconos;
					when  5 => p2(31 downto  0) <= o_RAMData_reconos;
					when  6 => -- p3 is ignored
					when  7 => -- - ^^ -
					when  8 => p4(63 downto 32) <= o_RAMData_reconos;
					when  9 => p4(31 downto  0) <= o_RAMData_reconos;
					when 10 => u0(63 downto 32) <= o_RAMData_reconos;
					when 11 => u0(31 downto  0) <= o_RAMData_reconos;
					when 12 => x0(63 downto 32) <= o_RAMData_reconos;
					when 13 => x0(31 downto  0) <= o_RAMData_reconos;
					when 14 => x1(63 downto 32) <= o_RAMData_reconos;
					when 15 => x1(31 downto  0) <= o_RAMData_reconos;
					when 16 => t (63 downto 32) <= o_RAMData_reconos;
					when 17 => t (31 downto  0) <= o_RAMData_reconos;
					when others => -- do nothing
				end case;
			else
				--TODO we should copy them while out_tvalid is high!
				if out_tvalid = '1' then
					case conv_integer(o_RAMAddr_reconos) is
						when 18 => i_RAMData_reconos <= dx0(63 downto 32);
						when 19 => i_RAMData_reconos <= dx0(31 downto  0);
						when 20 => i_RAMData_reconos <= dx1(63 downto 32);
						when 21 => i_RAMData_reconos <= dx1(31 downto  0);
						when others => i_RAMData_reconos <= (others => 'U');
					end case;
				else
					i_RAMData_reconos <= (others => 'U');
				end if;
			end if;
		end if;
	end process;

	-- instantiate bubble_sorter module
	single_pendulum_i : single_pendulum
		port map (
			aclk => clk,
			x0   => x0,
			x1   => x1,
			u0   => u0,
			t    => t,
			p0   => p0,
			p1   => p1,
			p2   => p2,
			p4   => p4,
			dx0  => dx0,
			dx1  => dx1,
			in_tvalid  => in_tvalid,
			in_tready  => in_tready,
			out_tvalid => out_tvalid,
			out_tready => out_tready
	);

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
	
	o_RAMAddr_reconos(0 to C_LOCAL_RAM_ADDRESS_WIDTH-1) <= o_RAMAddr_reconos_2((32-C_LOCAL_RAM_ADDRESS_WIDTH) to 31);
		
	-- os and memory synchronisation state machine
	reconos_fsm: process (clk,rst,o_osif,o_memif,o_ram) is
		variable done  : boolean;
	begin
		if rst = '1' then
			osif_reset(o_osif);
			memif_reset(o_memif);
			ram_reset(o_ram);
			state <= STATE_GET_ADDR;
			done  := False;
			addr <= (others => '0');
			len <= (others => '0');
			sort_start <= '0';
			in_tvalid <= '0';
			out_tready <= '0';
		elsif rising_edge(clk) then
			case state is

				-- get address via mbox: the data will be copied from this address to the local ram in the next states
				when STATE_GET_ADDR =>
					osif_mbox_get(i_osif, o_osif, MBOX_RECV, addr, done);
					if done then
						if (addr = X"FFFFFFFF") then
							state <= STATE_THREAD_EXIT;
						elsif (addr = X"FFFFFFFE") then
							-- skip reading from memory - only for performance tests!
							state <= STATE_STARTING;
						else
							len               <= conv_std_logic_vector(8 * C_INPUT_SIZE,24);
							addr              <= addr(31 downto 2) & "00";
							state             <= STATE_READ;
						end if;
					end if;
					in_tvalid <= '0';
					out_tready <= '1';
				
				-- copy data from main memory to local memory
				when STATE_READ =>
					-- Throw away any results that are still in memory.
					out_tready <= '1';

					-- We assume that in_tready won't go low, after it 
					if in_tready = '1' then
						memif_read(i_ram,o_ram,i_memif,o_memif,addr,X"00000000",len,done);
						if done then
							sort_start <= '1';
							state <= STATE_STARTING;
						end if;
					end if;

				when STATE_STARTING =>
					in_tvalid <= '1';

					if in_tready = '1' then
						state <= STATE_EVAL;
					end if;

				-- calculate state derivatives
				when STATE_EVAL =>
					in_tvalid <= '0';

					if out_tvalid = '1' then
						--TODO copy output data to temporary storage?
						state        <= STATE_WRITE;
						len          <= conv_std_logic_vector(8 * C_OUTPUT_SIZE,24);
						addr_outputs <= (addr(31 downto 2) & "00") + conv_std_logic_vector(8 * C_INPUT_SIZE,32);

						out_tready <= '0';
					end if;
					
				-- copy data from local memory to main memory
				when STATE_WRITE =>
					if (addr = X"FFFFFFFE") then
						-- skip writing to memory - only for performance tests!
						state <= STATE_ACK;
					else
						memif_write(i_ram,o_ram,i_memif,o_memif,X"00000000",addr_outputs,len,done);
						if done then
							state <= STATE_ACK;
						end if;
					end if;
				
				-- send mbox that signals that the sorting is finished
				when STATE_ACK =>
					osif_mbox_put(i_osif, o_osif, MBOX_SEND, addr, ignore, done);
					if done then state <= STATE_GET_ADDR; end if;

				-- thread exit
				when STATE_THREAD_EXIT =>
					osif_thread_exit(i_osif,o_osif);
			
			end case;
		end if;
	end process;
	
end architecture;
