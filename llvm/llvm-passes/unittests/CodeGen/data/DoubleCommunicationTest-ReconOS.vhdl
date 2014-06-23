-- generated by Mehari
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.math_real.ALL;
use ieee.numeric_std.all;

--TODO
--library proc_common_v3_00_a;
--use proc_common_v3_00_a.proc_common_pkg.all;

library reconos_v3_01_a;
use reconos_v3_01_a.reconos_pkg.all;

entity reconos is
   port ( 
         OSIF_FIFO_Sw2Hw_Data : in  std_logic_vector(31 downto 0);
         OSIF_FIFO_Sw2Hw_Fill : in  std_logic_vector(15 downto 0);
         OSIF_FIFO_Sw2Hw_Empty : in std_logic;
         OSIF_FIFO_Sw2Hw_RE : out std_logic;
         OSIF_FIFO_Hw2Sw_Data : out  std_logic_vector(31 downto 0);
         OSIF_FIFO_Hw2Sw_Rem : in  std_logic_vector(15 downto 0);
         OSIF_FIFO_Hw2Sw_Full : in std_logic;
         OSIF_FIFO_Hw2Sw_WE : out std_logic;
         MEMIF_FIFO_Hwt2Mem_Data : out  std_logic_vector(31 downto 0);
         MEMIF_FIFO_Hwt2Mem_Rem : in  std_logic_vector(15 downto 0);
         MEMIF_FIFO_Hwt2Mem_Full : in std_logic;
         MEMIF_FIFO_Hwt2Mem_WE : out std_logic;
         MEMIF_FIFO_Mem2Hwt_Data : in  std_logic_vector(31 downto 0);
         MEMIF_FIFO_Mem2Hwt_Fill : in  std_logic_vector(15 downto 0);
         MEMIF_FIFO_Mem2Hwt_Empty : in std_logic;
         MEMIF_FIFO_Mem2Hwt_RE : out std_logic;
         HWT_Clk : in std_logic;
         HWT_Rst : in std_logic
   );
   attribute SIGIS : string;
   attribute SIGIS of HWT_Clk : signal is "Clk";
   attribute SIGIS of HWT_Rst : signal is "Rst";
end entity;

architecture arch of reconos is
   component test is
      port ( 
         aclk : in std_logic;
         reset : in std_logic;
         a_in_data : in  std_logic_vector(63 downto 0);
         a_in_valid : in std_logic;
         a_in_ready : out std_logic;
         a_mbox_data : out  std_logic_vector(63 downto 0);
         a_mbox_valid : out std_logic;
         a_mbox_ready : in std_logic;
         t0_mbox_data : in  std_logic_vector(63 downto 0);
         t0_mbox_valid : in std_logic;
         t0_mbox_ready : out std_logic;
         a_out_data : out  std_logic_vector(63 downto 0);
         a_out_valid : out std_logic;
         a_out_ready : in std_logic
   );
   end component;

signal clk : std_logic;
signal rst : std_logic;
signal init : std_logic;
signal a_in_data :  std_logic_vector(63 downto 0);
signal a_in_valid : std_logic;
signal a_in_valid_component_accepted_the_value : std_logic;
signal a_in_valid_direct : std_logic;
signal a_in_ready : std_logic;
signal a_mbox_data :  std_logic_vector(63 downto 0);
signal a_mbox_valid : std_logic;
signal a_mbox_ready : std_logic;
signal t0_mbox_data :  std_logic_vector(63 downto 0);
signal t0_mbox_valid : std_logic;
signal t0_mbox_valid_component_accepted_the_value : std_logic;
signal t0_mbox_valid_direct : std_logic;
signal t0_mbox_ready : std_logic;
signal a_out_data :  std_logic_vector(63 downto 0);
signal a_out_valid : std_logic;
signal a_out_ready : std_logic;
   type STATE_TYPE is (
      STATE_INIT, STATE_MBOX_READ_a_in_data1, STATE_MBOX_READ_a_in_data2, STATE_MBOX_WRITE_a_mbox_data_wait, 
      STATE_MBOX_WRITE_a_mbox_data1, STATE_MBOX_WRITE_a_mbox_data2, STATE_MBOX_READ_t0_mbox_data1, STATE_MBOX_READ_t0_mbox_data2, 
      STATE_MBOX_WRITE_a_out_data_wait, STATE_MBOX_WRITE_a_out_data1, STATE_MBOX_WRITE_a_out_data2, STATE_THREAD_EXIT, 
      STATE_FINISH);
   signal state        : STATE_TYPE;
   signal i_osif       : i_osif_t;
   signal o_osif       : o_osif_t;
   signal i_memif      : i_memif_t;
   signal o_memif      : o_memif_t;
   signal i_ram        : i_ram_t;
   signal o_ram        : o_ram_t;
   signal addr      : std_logic_vector(31 downto 0);
   signal len       : std_logic_vector(23 downto 0);
   signal ignore    : std_logic_vector(31 downto 0);
   signal o_RAMAddr : std_logic_vector(0 to 31);
   signal o_RAMData : std_logic_vector(0 to 31);
   signal o_RAMWE   : std_logic;
   signal i_RAMData : std_logic_vector(0 to 31);
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
     o_RAMAddr,
     o_RAMWE,
     o_RAMData,
     i_RAMData
   );

   calculation: test
      port map ( a_in_data => a_in_data,
                 a_in_ready => a_in_ready,
                 a_in_valid => a_in_valid_direct,
                 a_mbox_data => a_mbox_data,
                 a_mbox_ready => a_mbox_ready,
                 a_mbox_valid => a_mbox_valid,
                 a_out_data => a_out_data,
                 a_out_ready => a_out_ready,
                 a_out_valid => a_out_valid,
                 aclk => clk,
                 reset => rst,
                 t0_mbox_data => t0_mbox_data,
                 t0_mbox_ready => t0_mbox_ready,
                 t0_mbox_valid => t0_mbox_valid_direct);
  local_fake_ram_ctrl : process (clk) is
  begin
    if (rising_edge(clk)) then
      if (o_RAMWE = '1') then
        case conv_integer(o_RAMAddr) is
          when others => -- do nothing
        end case;
      else
        case conv_integer(o_RAMAddr) is
          when others => i_RAMData <= (others => 'U');
        end case;
      end if;
    end if;
  end process;
   a_in_valid_valid: process (clk, rst, a_in_valid) is
   begin
      if init = '1' then
         a_in_valid_direct <= '0';
         a_in_valid_component_accepted_the_value <= '0';
      elsif rising_edge(clk) then
         if a_in_valid_component_accepted_the_value = '1' then
            a_in_valid_direct <= '0';
         elsif a_in_valid = '1' and a_in_ready = '1' then
            a_in_valid_direct <= '1';
            a_in_valid_component_accepted_the_value <= '1';
         else
            a_in_valid_direct <= a_in_valid;
         end if;
      end if;
   end process;
   t0_mbox_valid_valid: process (clk, rst, t0_mbox_valid) is
   begin
      if init = '1' then
         t0_mbox_valid_direct <= '0';
         t0_mbox_valid_component_accepted_the_value <= '0';
      elsif rising_edge(clk) then
         if t0_mbox_valid_component_accepted_the_value = '1' then
            t0_mbox_valid_direct <= '0';
         elsif t0_mbox_valid = '1' and t0_mbox_ready = '1' then
            t0_mbox_valid_direct <= '1';
            t0_mbox_valid_component_accepted_the_value <= '1';
         else
            t0_mbox_valid_direct <= t0_mbox_valid;
         end if;
      end if;
   end process;
   -- os and memory synchronisation state machine
   reconos_fsm: process (clk,rst,o_osif,o_memif,o_ram) is
     variable done  : boolean;
   --TODO remove
     --procedure goto_read(dummy:integer) is
     --begin
     --  len   <= conv_std_logic_vector(8 * C_INPUT_SIZE,24);
     --  addr  <= addr(31 downto 2) & "00";
     --  state <= STATE_READ;
     --end;
   begin
      if rst = '1' then
          osif_reset(o_osif);
          memif_reset(o_memif);
          ram_reset(o_ram);
          state <= STATE_INIT;
          done  := False;
          addr <= (others => '0');
          len <= (others => '0');
          init <= '1';

         a_in_valid <= '0';
         a_mbox_ready <= '0';
         t0_mbox_valid <= '0';
         a_out_ready <= '0';

      elsif rising_edge(clk) then
         case state is
            -- get address via mbox: the data will be copied from this address to the local ram in the next states
            when STATE_INIT =>
               init <= '1';
               osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(0, 32)), addr, done);
               if done then
                 init <= '0';
                 if (addr = X"FFFFFFFF") then
                   state <= STATE_THREAD_EXIT;
                 -- elsif (addr = X"FFFFFFFE") then
                 --   state <= STATE_READ;
                 --elsif (addr = X"FFFFFFFD") then
                 --  state <= STATE_READ_ITERATIONS;
                 else
                   state <= STATE_MBOX_READ_a_in_data1;
                 end if;
               end if;
               
               a_in_valid <= '0';
               a_mbox_ready <= '0';
               t0_mbox_valid <= '0';
               a_out_ready <= '0';

            when STATE_MBOX_READ_a_in_data1 =>
               osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(0, 32)), a_in_data(31 downto 0), done);
               if done then
                 state <= STATE_MBOX_READ_a_in_data2;
               end if;

            when STATE_MBOX_READ_a_in_data2 =>
               osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(0, 32)), a_in_data(63 downto 32), done);
               if done then
                  a_in_valid <= '1';
                 state <= STATE_MBOX_WRITE_a_mbox_data_wait;
               end if;

            when STATE_MBOX_WRITE_a_mbox_data_wait =>
               a_mbox_ready <= '1';
               if a_mbox_valid = '1' then
                  osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(3, 32)), a_mbox_data(31 downto 0), ignore, done);
                  state <= STATE_MBOX_WRITE_a_mbox_data1;
               end if;
            when STATE_MBOX_WRITE_a_mbox_data1 =>
               osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(3, 32)), a_mbox_data(31 downto 0), ignore, done);
               if done then
                 a_mbox_ready <= '0';
                 state <= STATE_MBOX_WRITE_a_mbox_data2;
               end if;

            when STATE_MBOX_WRITE_a_mbox_data2 =>
               osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(3, 32)), a_mbox_data(63 downto 32), ignore, done);
               if done then
                 a_mbox_ready <= '0';
                 state <= STATE_MBOX_READ_t0_mbox_data1;
               end if;

            when STATE_MBOX_READ_t0_mbox_data1 =>
               osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(2, 32)), t0_mbox_data(31 downto 0), done);
               if done then
                 state <= STATE_MBOX_READ_t0_mbox_data2;
               end if;

            when STATE_MBOX_READ_t0_mbox_data2 =>
               osif_mbox_get(i_osif, o_osif, std_logic_vector(to_unsigned(2, 32)), t0_mbox_data(63 downto 32), done);
               if done then
                  t0_mbox_valid <= '1';
                 state <= STATE_MBOX_WRITE_a_out_data_wait;
               end if;

            when STATE_MBOX_WRITE_a_out_data_wait =>
               a_out_ready <= '1';
               if a_out_valid = '1' then
                  osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(1, 32)), a_out_data(31 downto 0), ignore, done);
                  state <= STATE_MBOX_WRITE_a_out_data1;
               end if;
            when STATE_MBOX_WRITE_a_out_data1 =>
               osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(1, 32)), a_out_data(31 downto 0), ignore, done);
               if done then
                 a_out_ready <= '0';
                 state <= STATE_MBOX_WRITE_a_out_data2;
               end if;

            when STATE_MBOX_WRITE_a_out_data2 =>
               osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(1, 32)), a_out_data(63 downto 32), ignore, done);
               if done then
                 a_out_ready <= '0';
                 state <= STATE_FINISH;
               end if;

            -- thread exit
            when STATE_THREAD_EXIT =>
               osif_thread_exit(i_osif,o_osif);

            when STATE_FINISH =>
               --if iterations = X"00000000" then
                 osif_mbox_put(i_osif, o_osif, std_logic_vector(to_unsigned(1, 32)), addr, ignore, done);
                 if done then
                   state <= STATE_INIT;
                 end if;
               --else
               --  iterations <= iterations - X"00000001";
               --  goto_read(0);
               --end if;

         end case;
      end if;
   end process;
end architecture;

