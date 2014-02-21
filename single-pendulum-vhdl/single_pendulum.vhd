----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    11:22:41 12/16/2013 
-- Design Name: 
-- Module Name:    single_pendulum - Structural 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity single_pendulum is
    Port ( x0, x1, u0, t  : in  STD_LOGIC_VECTOR(63 downto 0);
           p0, p1, p2, p4 : in  STD_LOGIC_VECTOR(63 downto 0);
           dx0, dx1       : out STD_LOGIC_VECTOR(63 downto 0);
           in_tvalid      : in  STD_LOGIC;
           in_tready      : out STD_LOGIC;
           out_tvalid     : out STD_LOGIC;
           out_tready     : in  STD_LOGIC;
           aclk           : in  STD_LOGIC );
end single_pendulum;

architecture Structural of single_pendulum is

    component float_neg is
        Port ( aclk : IN STD_LOGIC;
               a_tvalid : in  STD_LOGIC;
               a_tready : out  STD_LOGIC;
               a_tdata : in  STD_LOGIC_VECTOR (63 downto 0);
               result_tvalid : out  STD_LOGIC;
               result_tready : in  STD_LOGIC;
               result_tdata : out  STD_LOGIC_VECTOR (63 downto 0));
    end component;
    component float_sin is
        Port( aclk : IN STD_LOGIC;
              a_tdata : IN STD_LOGIC_VECTOR (63 downto 0);
              a_tvalid : IN STD_LOGIC;
              a_tready : OUT STD_LOGIC;
              result_tdata : OUT STD_LOGIC_VECTOR (63 downto 0);
              result_tvalid : OUT STD_LOGIC;
              result_tready : IN STD_LOGIC );
    end component;
    COMPONENT float_mul
      PORT (
        aclk : IN STD_LOGIC;
        s_axis_a_tvalid : IN STD_LOGIC;
        s_axis_a_tready : OUT STD_LOGIC;
        s_axis_a_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        s_axis_b_tvalid : IN STD_LOGIC;
        s_axis_b_tready : OUT STD_LOGIC;
        s_axis_b_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        m_axis_result_tvalid : OUT STD_LOGIC;
        m_axis_result_tready : IN STD_LOGIC;
        m_axis_result_tdata : OUT STD_LOGIC_VECTOR(63 DOWNTO 0)
      );
    END COMPONENT;
    COMPONENT float_div
      PORT (
        aclk : IN STD_LOGIC;
        s_axis_a_tvalid : IN STD_LOGIC;
        s_axis_a_tready : OUT STD_LOGIC;
        s_axis_a_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        s_axis_b_tvalid : IN STD_LOGIC;
        s_axis_b_tready : OUT STD_LOGIC;
        s_axis_b_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        m_axis_result_tvalid : OUT STD_LOGIC;
        m_axis_result_tready : IN STD_LOGIC;
        m_axis_result_tdata : OUT STD_LOGIC_VECTOR(63 DOWNTO 0)
      );
    END COMPONENT;
    COMPONENT float_add
      PORT (
        aclk : IN STD_LOGIC;
        s_axis_a_tvalid : IN STD_LOGIC;
        s_axis_a_tready : OUT STD_LOGIC;
        s_axis_a_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        s_axis_b_tvalid : IN STD_LOGIC;
        s_axis_b_tready : OUT STD_LOGIC;
        s_axis_b_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        m_axis_result_tvalid : OUT STD_LOGIC;
        m_axis_result_tready : IN STD_LOGIC;
        m_axis_result_tdata : OUT STD_LOGIC_VECTOR(63 DOWNTO 0)
      );
    END COMPONENT;
    COMPONENT float_sub
      PORT (
        aclk : IN STD_LOGIC;
        s_axis_a_tvalid : IN STD_LOGIC;
        s_axis_a_tready : OUT STD_LOGIC;
        s_axis_a_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        s_axis_b_tvalid : IN STD_LOGIC;
        s_axis_b_tready : OUT STD_LOGIC;
        s_axis_b_tdata : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        m_axis_result_tvalid : OUT STD_LOGIC;
        m_axis_result_tready : IN STD_LOGIC;
        m_axis_result_tdata : OUT STD_LOGIC_VECTOR(63 DOWNTO 0)
      );
    END COMPONENT;

    signal a, b, c, d, e, f, g, h : STD_LOGIC_VECTOR (63 downto 0);
    signal a_valid, b_valid, c_valid, d_valid, e_valid, f_valid, g_valid, h_valid : STD_LOGIC;
    signal a_ready, b_ready, c_ready, d_ready, e_ready, f_ready, g_ready, h_ready : STD_LOGIC;
    signal x0_ready, x1_ready, u0_ready, t_ready, p0_ready, p1_ready, p2_ready, p4_ready : STD_LOGIC;

begin
    in_tready <= x0_ready and x1_ready and u0_ready and t_ready and p0_ready and p1_ready and p2_ready and p4_ready;
    t_ready <= '1';

    -- dx[0] = x[1];
    dx0_process: process(aclk)
    begin
      if rising_edge(aclk) and in_tvalid = '1' then
        dx0 <= x1;
      end if;
    end process;

    -- a = NEG(Vp[2])
    t_a : float_neg
      PORT MAP (
        aclk => aclk,
        a_tvalid => in_tvalid,
        a_tready => p2_ready,
        a_tdata  => p2,
        result_tvalid => a_valid,
        result_tready => a_ready,
        result_tdata  => a
      );

    -- b = a*Vp[1]
    t_b : float_mul
      PORT MAP (
        aclk => aclk,
        s_axis_a_tvalid => a_valid,
        s_axis_a_tready => a_ready,
        s_axis_a_tdata  => a,
        s_axis_b_tvalid => in_tvalid,
        s_axis_b_tready => p1_ready,
        s_axis_b_tdata  => p1,
        m_axis_result_tvalid => b_valid,
        m_axis_result_tready => b_ready,
        m_axis_result_tdata  => b
      );
    
    -- c = SIN(x[0])
    t_c : float_sin
      PORT MAP (
        aclk          => aclk,
        a_tdata       => x0,
        a_tvalid      => in_tvalid,
        a_tready      => x0_ready,
        result_tdata  => c,
        result_tvalid => c_valid,
        result_tready => c_ready
      );

    -- d = b*c
    t_d : float_mul
      PORT MAP (
        aclk => aclk,
        s_axis_a_tvalid => b_valid,
        s_axis_a_tready => b_ready,
        s_axis_a_tdata  => b,
        s_axis_b_tvalid => c_valid,
        s_axis_b_tready => c_ready,
        s_axis_b_tdata  => c,
        m_axis_result_tvalid => d_valid,
        m_axis_result_tready => d_ready,
        m_axis_result_tdata  => d
      );

    -- e = x[1]*Vp[0]
    t_e : float_mul
      PORT MAP (
        aclk => aclk,
        s_axis_a_tvalid => in_tvalid,
        s_axis_a_tready => x1_ready,
        s_axis_a_tdata  => x1,
        s_axis_b_tvalid => in_tvalid,
        s_axis_b_tready => p0_ready,
        s_axis_b_tdata  => p0,
        m_axis_result_tvalid => e_valid,
        m_axis_result_tready => e_ready,
        m_axis_result_tdata  => e
      );

    -- f = d-e
    t_f : float_sub
      PORT MAP (
        aclk => aclk,
        s_axis_a_tvalid => d_valid,
        s_axis_a_tready => d_ready,
        s_axis_a_tdata  => d,
        s_axis_b_tvalid => e_valid,
        s_axis_b_tready => e_ready,
        s_axis_b_tdata  => e,
        m_axis_result_tvalid => f_valid,
        m_axis_result_tready => f_ready,
        m_axis_result_tdata  => f
      );

    -- g = f+u[0]
    t_g : float_add
      PORT MAP (
        aclk => aclk,
        s_axis_a_tvalid => f_valid,
        s_axis_a_tready => f_ready,
        s_axis_a_tdata  => f,
        s_axis_b_tvalid => in_tvalid,
        s_axis_b_tready => u0_ready,
        s_axis_b_tdata  => u0,
        m_axis_result_tvalid => g_valid,
        m_axis_result_tready => g_ready,
        m_axis_result_tdata  => g
      );

    -- dx[1] = g/Vp[4]
    t_dx1 : float_div
      PORT MAP (
        aclk => aclk,
        s_axis_a_tvalid => g_valid,
        s_axis_a_tready => g_ready,
        s_axis_a_tdata  => g,
        s_axis_b_tvalid => in_tvalid,
        s_axis_b_tready => p4_ready,
        s_axis_b_tdata  => p4,
        m_axis_result_tvalid => h_valid,
        m_axis_result_tready => h_ready,
        m_axis_result_tdata  => dx1
      );

    out_tvalid <= h_valid;
    h_ready <= out_tready;

end Structural;

