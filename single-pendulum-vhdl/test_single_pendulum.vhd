library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use ieee.math_real.all;

library work;
use work.double_type.all;
use work.float_helpers.all;
use work.test_helpers.all;


entity test_single_pendulum is
end test_single_pendulum;

architecture behavior of test_single_pendulum is 

  -- Component Declaration for the Unit Under Test (UUT)

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

  -- Clock
  signal aclk : std_logic := '0';
  constant aclk_period : time := 10 ns;

  -- Inputs
  signal input_x0_tdata  : double    := (others => '0');
  signal input_x0_tvalid : std_logic := '0';
  signal input_x1_tdata  : double    := (others => '0');
  signal input_x1_tvalid : std_logic := '0';
  signal input_u0_tdata  : double    := (others => '0');
  signal input_u0_tvalid : std_logic := '0';
  signal input_t_tdata   : double    := (others => '0');
  signal input_t_tvalid  : std_logic := '0';
  signal input_p0_tdata  : double    := (others => '0');
  signal input_p0_tvalid : std_logic := '0';
  signal input_p1_tdata  : double    := (others => '0');
  signal input_p1_tvalid : std_logic := '0';
  signal input_p2_tdata  : double    := (others => '0');
  signal input_p2_tvalid : std_logic := '0';
  signal input_p4_tdata  : double    := (others => '0');
  signal input_p4_tvalid : std_logic := '0';
  signal input_tvalid : std_logic := '0';
  signal result_tready: std_logic := '0';

  -- Outputs
  --signal input_x0_tready  : std_logic;
  --signal input_x1_tready  : std_logic;
  --signal input_u0_tready  : std_logic;
  --signal input_t_tready   : std_logic;
  --signal input_p0_tready  : std_logic;
  --signal input_p1_tready  : std_logic;
  --signal input_p2_tready  : std_logic;
  --signal input_p4_tready  : std_logic;
  signal input_tready  : std_logic;
  signal result_dx0_tdata, result_dx1_tdata  : double;
  signal result_tvalid : std_logic;

BEGIN
  -- Instantiate the Unit Under Test (UUT)
  uut: single_pendulum PORT MAP (
    aclk => aclk,
    -- s_axis_x0_tdata  => input_x0_tdata,
    -- s_axis_x0_tvalid => input_x0_tvalid,
    -- s_axis_x0_tready => input_x0_tready,
    -- s_axis_x1_tdata  => input_x1_tdata,
    -- s_axis_x1_tvalid => input_x1_tvalid,
    -- s_axis_x1_tready => input_x1_tready,
    -- s_axis_u0_tdata  => input_u0_tdata,
    -- s_axis_u0_tvalid => input_u0_tvalid,
    -- s_axis_u0_tready => input_u0_tready,
    -- s_axis_t_tdata   => input_t_tdata,
    -- s_axis_t_tvalid  => input_t_tvalid,
    -- s_axis_t_tready  => input_t_tready,
    -- s_axis_p0_tdata  => input_p0_tdata,
    -- s_axis_p0_tvalid => input_p0_tvalid,
    -- s_axis_p0_tready => input_p0_tready,
    -- s_axis_p1_tdata  => input_p1_tdata,
    -- s_axis_p1_tvalid => input_p1_tvalid,
    -- s_axis_p1_tready => input_p1_tready,
    -- s_axis_p2_tdata  => input_p2_tdata,
    -- s_axis_p2_tvalid => input_p2_tvalid,
    -- s_axis_p2_tready => input_p2_tready,
    -- s_axis_p4_tdata  => input_p4_tdata,
    -- s_axis_p4_tvalid => input_p4_tvalid,
    -- s_axis_p4_tready => input_p4_tready,
    -- m_axis_result_tdata   => result_tdata,
    -- m_axis_result_tvalid  => result_tvalid,
    -- m_axis_result_tready  => result_tready

    x0 => input_x0_tdata,
    x1 => input_x1_tdata,
    u0 => input_u0_tdata,
    t  => input_t_tdata,
    p0 => input_p0_tdata,
    p1 => input_p1_tdata,
    p2 => input_p2_tdata,
    p4 => input_p4_tdata,

    dx0 => result_dx0_tdata,
    dx1 => result_dx1_tdata,

    in_tvalid  => input_tvalid,
    in_tready  => input_tready,
    out_tvalid => result_tvalid,
    out_tready => result_tready
  );

  -- Clock process definitions
  aclk_process : process
  begin
    aclk <= '0';
    wait for aclk_period/2;
    aclk <= '1';
    wait for aclk_period/2;
  end process;

  stimulus_process : process
    constant max_clock_cycles : natural := 10000;

    procedure test(p0_value, p1_value, p2_value, p4_value : in real;
                   u0_value, x0_value, x1_value, t_value : in real;
                   expected_dx0_value, expected_dx1_value : in real;
                   epsilon : in real := 1.0e-10) is
    begin
      wait until input_tready = '1' and rising_edge(aclk) for 10*aclk_period;
      assert input_tready = '1'
        report "uut is not ready for data: input_tready = "
          & std_logic'image(input_tready);

      wait until falling_edge(aclk);
      report "testing with:";
      report "  x0: " & real'image(x0_value);
      report "  x1: " & real'image(x1_value);
      report "  u0: " & real'image(u0_value);
      report "  t:  " & real'image(t_value);
      report "  p0: " & real'image(p0_value);
      report "  p1: " & real'image(p1_value);
      report "  p2: " & real'image(p2_value);
      report "  p4: " & real'image(p4_value);
      --report "  result_tvalid (before calculation): " & std_logic'image(result_tvalid);
      input_x0_tdata <= to_float(x0_value);
      input_x1_tdata <= to_float(x1_value);
      input_u0_tdata <= to_float(u0_value);
      input_t_tdata  <= to_float(t_value);
      input_p0_tdata <= to_float(p0_value);
      input_p1_tdata <= to_float(p1_value);
      input_p2_tdata <= to_float(p2_value);
      input_p4_tdata <= to_float(p4_value);
      input_x0_tvalid <= '1';
      input_x1_tvalid <= '1';
      input_u0_tvalid <= '1';
      input_t_tvalid  <= '1';
      input_p0_tvalid <= '1';
      input_p1_tvalid <= '1';
      input_p2_tvalid <= '1';
      input_p4_tvalid <= '1';
      input_tvalid <= '1';

      if max_clock_cycles > 0 then
        wait for aclk_period;

        assert result_tvalid = '0'
          report "result is still ready (although it shouldn't be): result_tvalid = " & std_logic'image(result_tvalid);

        input_x0_tvalid <= '0';
        input_x1_tvalid <= '0';
        input_u0_tvalid <= '0';
        input_t_tvalid  <= '0';
        input_p0_tvalid <= '0';
        input_p1_tvalid <= '0';
        input_p2_tvalid <= '0';
        input_p4_tvalid <= '0';
        input_x0_tdata <= (others => '0');
        input_x1_tdata <= (others => '0');
        input_u0_tdata <= (others => '0');
        input_t_tdata  <= (others => '0');
        input_p0_tdata <= (others => '0');
        input_p1_tdata <= (others => '0');
        input_p2_tdata <= (others => '0');
        input_p4_tdata <= (others => '0');
        input_tvalid <= '0';

        wait until result_tvalid = '1' and rising_edge(aclk)
          for max_clock_cycles*aclk_period - aclk_period/2;
      else
        wait for aclk_period/2;
      end if;
      assert result_tvalid = '1'
        report "result was not ready in time: result_tvalid = " & std_logic'image(result_tvalid);

      if result_tvalid = '1' then
        --report "  result_tvalid (after calculation): " & std_logic'image(result_tvalid);
        assertAlmostEqual(to_real(result_dx0_tdata), expected_dx0_value, epsilon);
        assertAlmostEqual(to_real(result_dx1_tdata), expected_dx1_value, epsilon);
      end if;
    end procedure;
  begin
    wait for 100 ns;

    result_tready <= '1';

    wait for aclk_period*10;

    --   p0   p1    p2   p4   u0     x0        x1         t    d0         d1
    test(0.2, 9.81, 0.5, 0.2, 0.0  , 1.998999, -0.222032, 0.0, -0.222032,  -22.08869242579079 );
    test(0.2, 9.81, 0.5, 0.2, 0.0  , -1.3    , -0.222032, 0.0, -0.222032,  23.853296497356656 );
    test(0.2, 9.81, 0.5, 0.2, 0.0  , 1.998999, 2.0      , 0.0, 2.0      ,  -24.310724425790788);
    test(0.2, 1.62, 0.5, 0.2, 0.0  , 1.998999, -0.222032, 0.0, -0.222032,  -3.462307813433341 );
    test(0.2, 9.81, 2.3, 0.2, 0.0  , 1.998999, -0.222032, 0.0, -0.222032,  -102.4073003586376 );
    test(0.2, 9.81, 0.5, 0.2, 0.0  , 1.998999, -0.222032, 0.0, -0.222032,  -22.08869242579079 );
    test(0.2, 9.81, 0.5, 0.7, 0.0  , 1.998999, -0.222032, 0.0, -0.222032,  -6.311054978797369 );
    test(0.2, 9.81, 0.5, 0.2, -17.0, 1.998999, -0.222032, 0.0, -0.222032,  -107.08869242579078);

    -- The user might want to see what happens after the tests, so we make
    -- sure that the waveform file contains a few more samples.
    wait for 1 ns;

    endOfSimulation(0);

    wait;
  end process;

  input_tready_is_not_undefined : process
  begin
    -- signal may be undefined at the start of the simulation, but not for long
    if now < 1ps then
      wait for 1ps;
    end if;

    wait on input_tready;
    assert input_tready = '0' or input_tready = '1'
      report "input_tready must be either '0' or '1' all the time, but it is " & std_logic'image(input_tready);
  end process;

--  debug_timing_process : process(aclk)
--    variable input_a_was_valid : std_logic := '0';
--    variable input_b_was_valid : std_logic := '0';
--    variable output_was_valid  : std_logic := '0';
--  begin
--    if rising_edge(aclk) then
--      if input_a_tvalid = '1' and input_a_was_valid = '0' then
--        report "data on input a is valid";
--      end if;
--
--      if input_b_tvalid = '1' and input_b_was_valid = '0' then
--        report "data on input b is valid";
--      end if;
--
--      if result_tvalid = '1' and output_was_valid = '0' then
--        report "output data is valid";
--      end if;
--
--      input_a_was_valid := input_a_tvalid;
--      input_b_was_valid := input_b_tvalid;
--      output_was_valid  := result_tvalid;
--    end if;
--  end process;
end;
