LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
use ieee.MATH_REAL.all;

LIBRARY work;
use work.double_type.all;

-- This is a (modified) extract from float_pkg_c.vhdl. I have modified it to work
-- with my double type instead of their more general float type.
-- see http://www.eda.org/fphdl/float_pkg_c.vhdl

package float_pkg_c_min is
  function to_float (
      arg                     : REAL;
      constant exponent_width : NATURAL    := 11;  -- length of FP output exponent
      constant fraction_width : NATURAL    := 52;  -- length of FP output fraction
      --constant round_style    : round_type := float_round_style;  -- rounding option
      constant denormalize    : BOOLEAN    := true)  -- Use IEEE extended FP
      return double;

  -- float to real
  function to_real (
    arg                  : double;  -- floating point input
    constant check_error : BOOLEAN    := true;  -- check for errors
    constant denormalize : BOOLEAN    := true)  -- Use IEEE extended FP
    return REAL;
end package;

package body float_pkg_c_min is
  -- Generates the base number for the exponent normalization offset.
  function gen_expon_base (
    constant exponent_width : NATURAL)
    return SIGNED is
    variable result : SIGNED (exponent_width-1 downto 0);
  begin
    result                    := (others => '1');
    result (exponent_width-1) := '0';
    return result;
  end function gen_expon_base;

  -- purpose: AND all of the bits in a vector together
  -- This is a copy of the proposed "and_reduce" from 1076.3
  function and_reduce (arg : STD_ULOGIC_VECTOR)
    return STD_LOGIC is
    variable Upper, Lower : STD_ULOGIC;
    variable Half         : INTEGER;
    variable BUS_int      : STD_ULOGIC_VECTOR (arg'length - 1 downto 0);
    variable Result       : STD_ULOGIC;
  begin
    if (arg'length < 1) then            -- In the case of a NULL range
      Result := '1';
    else
      BUS_int := to_ux01 (arg);
      if (BUS_int'length = 1) then
        Result := BUS_int (BUS_int'left);
      elsif (BUS_int'length = 2) then
        Result := BUS_int (BUS_int'right) and BUS_int (BUS_int'left);
      else
        Half   := (BUS_int'length + 1) / 2 + BUS_int'right;
        Upper  := and_reduce (BUS_int (BUS_int'left downto Half));
        Lower  := and_reduce (BUS_int (Half - 1 downto BUS_int'right));
        Result := Upper and Lower;
      end if;
    end if;
    return Result;
  end function and_reduce;

  function and_reduce (arg : UNSIGNED)
    return STD_ULOGIC is
  begin
    return and_reduce (STD_ULOGIC_VECTOR (arg));
  end function and_reduce;

  function and_reduce (arg : SIGNED)
    return STD_ULOGIC is
  begin
    return and_reduce (STD_ULOGIC_VECTOR (arg));
  end function and_reduce;

  -- Integer version of the "log2" command (contributed by Peter Ashenden)
  function log2 (A : NATURAL) return NATURAL is
    variable quotient : NATURAL;
    variable result   : NATURAL := 0;
  begin
    quotient := A / 2;
    while quotient > 0 loop
      quotient := quotient / 2;
      result   := result + 1;
    end loop;
    return result;
  end function log2;

  -- Function similar to the ILOGB function in MATH_REAL
  function log2 (A : REAL) return INTEGER is
    variable Y : REAL;
    variable N : INTEGER := 0;
  begin
    if (A = 1.0 or A = 0.0) then
      return 0;
    end if;
    Y := A;
    if(A > 1.0) then
      while Y >= 2.0 loop
        Y := Y / 2.0;
        N := N + 1;
      end loop;
      return N;
    end if;
    -- O < Y < 1
    while Y < 1.0 loop
      Y := Y * 2.0;
      N := N - 1;
    end loop;
    return N;
  end function log2;

  -- types of boundary conditions
  type boundary_type is (normal, infinity, zero, denormal);

  -- purpose: Test the boundary conditions of a Real number
  procedure test_boundary (
    arg                     : in  REAL;     -- Input, converted to real
    constant fraction_width : in  NATURAL;  -- length of FP output fraction
    constant exponent_width : in  NATURAL;  -- length of FP exponent
    constant denormalize    : in  BOOLEAN := true;  -- Use IEEE extended FP
    variable btype          : out boundary_type;
    variable log2i          : out INTEGER
    ) is
    constant expon_base : SIGNED (exponent_width-1 downto 0) :=
      gen_expon_base(exponent_width);   -- exponent offset
    constant exp_min : SIGNED (12 downto 0) :=
      -(resize(expon_base, 13)) + 1;    -- Minimum normal exponent
    constant exp_ext_min : SIGNED (12 downto 0) :=
      exp_min - fraction_width;         -- Minimum for denormal exponent
    variable log2arg : INTEGER;         -- log2 of argument
  begin  -- function test_boundary
    -- Check to see if the exponent is big enough
    -- Note that the argument is always an absolute value at this point.
    log2arg := log2(arg);
    if arg = 0.0 then
      btype := zero;
    elsif exponent_width > 11 then      -- Exponent for Real is 11 (64 bit)
      btype := normal;
    else
      if log2arg < to_integer(exp_min) then
        if denormalize then
          if log2arg < to_integer(exp_ext_min) then
            btype := zero;
          else
            btype := denormal;
          end if;
        else
          if log2arg < to_integer(exp_min)-1 then
            btype := zero;
          else
            btype := normal;            -- Can still represent this number
          end if;
        end if;
      elsif exponent_width < 11 then
        if log2arg > to_integer(expon_base)+1 then
          btype := infinity;
        else
          btype := normal;
        end if;
      else
        btype := normal;
      end if;
    end if;
    log2i := log2arg;
  end procedure test_boundary;

  -- to_float (Real)
  -- typically not Synthesizable unless the input is a constant.
  function to_float (
    arg                     : REAL;
    constant exponent_width : NATURAL    := 11;  -- length of FP output exponent
    constant fraction_width : NATURAL    := 52;  -- length of FP output fraction
    --constant round_style    : round_type := float_round_style;  -- rounding option
    constant denormalize    : BOOLEAN    := true)  -- Use IEEE extended FP
    return double is
    variable result     : double (exponent_width + fraction_width downto 0);
    variable arg_real   : REAL;         -- Real version of argument
    variable validfp    : boundary_type;      -- Check for valid results
    variable exp        : INTEGER;      -- Integer version of exponent
    variable expon      : UNSIGNED (exponent_width - 1 downto 0);
                                        -- Unsigned version of exp.
    constant expon_base : SIGNED (exponent_width-1 downto 0) :=
      gen_expon_base(exponent_width);   -- exponent offset
    variable fract     : UNSIGNED (fraction_width-1 downto 0);
    variable frac      : REAL;          -- Real version of fraction
    constant roundfrac : REAL := 2.0 ** (-2 - fract'high);  -- used for rounding
    variable round     : BOOLEAN;       -- to round or not to round
  begin
    result   := (others => '0');
    arg_real := arg;
    if arg_real < 0.0 then
      result (exponent_width + fraction_width) := '1';
      arg_real                := - arg_real;  -- Make it positive.
    else
      result (exponent_width + fraction_width) := '0';
    end if;
    test_boundary (arg            => arg_real,
                   fraction_width => fraction_width,
                   exponent_width => exponent_width,
                   denormalize    => denormalize,
                   btype          => validfp,
                   log2i          => exp);
    if validfp = zero then
      return result;                    -- Result initialized to "0".
    elsif validfp = infinity then
      result (exponent_width - 1 + fraction_width downto fraction_width) := (others => '1');  -- Exponent all "1"
                                        -- return infinity.
      return result;
    else
      if validfp = denormal then        -- Exponent will default to "0".
        expon := (others => '0');
        frac  := arg_real * (2.0 ** (to_integer(expon_base)-1));
      else                              -- Number less than 1. "normal" number
        expon := UNSIGNED (to_signed (exp-1, exponent_width));
        expon(exponent_width-1) := not expon(exponent_width-1);
        frac := (arg_real / 2.0 ** exp) - 1.0;  -- Number less than 1.
      end if;
      for i in 0 to fract'high loop
        if frac >= 2.0 ** (-1 - i) then
          fract (fract'high - i) := '1';
          frac := frac - 2.0 ** (-1 - i);
        else
          fract (fract'high - i) := '0';
        end if;
      end loop;
      round := false;
      if frac > roundfrac or ((frac = roundfrac) and fract(0) = '1') then
        round := true;
      end if;
      if (round) then
        if and_reduce (fract) = '1' then      -- fraction is all "1"
          expon := expon + 1;
          fract := (others => '0');
        else
          fract := fract + 1;
        end if;
      end if;
      result (exponent_width-1 + fraction_width downto fraction_width) := double(expon);
      result (fraction_width-1 downto 0) := double(fract);
      return result;
    end if;
  end function to_float;

  -- purpose: Checks for a valid floating point number
  type valid_fpstate is (nan,           -- Signaling NaN (C FP_NAN)
                         quiet_nan,     -- Quiet NaN (C FP_NAN)
                         neg_inf,       -- Negative infinity (C FP_INFINITE)
                         neg_normal,    -- negative normalized nonzero
                         neg_denormal,  -- negative denormalized (FP_SUBNORMAL)
                         neg_zero,      -- -0 (C FP_ZERO)
                         pos_zero,      -- +0 (C FP_ZERO)
                         pos_denormal,  -- Positive denormalized (FP_SUBNORMAL)
                         pos_normal,    -- positive normalized nonzero
                         pos_inf,       -- positive infinity
                         isx);          -- at least one input is unknown

  function or_reduce (arg : STD_ULOGIC_VECTOR)
    return STD_LOGIC is
    variable Upper, Lower : STD_ULOGIC;
    variable Half         : INTEGER;
    variable BUS_int      : STD_ULOGIC_VECTOR (arg'length - 1 downto 0);
    variable Result       : STD_ULOGIC;
  begin
    if (arg'length < 1) then            -- In the case of a NULL range
      Result := '0';
    else
      BUS_int := to_ux01 (arg);
      if (BUS_int'length = 1) then
        Result := BUS_int (BUS_int'left);
      elsif (BUS_int'length = 2) then
        Result := BUS_int (BUS_int'right) or BUS_int (BUS_int'left);
      else
        Half   := (BUS_int'length + 1) / 2 + BUS_int'right;
        Upper  := or_reduce (BUS_int (BUS_int'left downto Half));
        Lower  := or_reduce (BUS_int (Half - 1 downto BUS_int'right));
        Result := Upper or Lower;
      end if;
    end if;
    return Result;
  end function or_reduce;

  function or_reduce (arg : UNSIGNED)
    return STD_ULOGIC is
  begin
    return or_reduce (STD_ULOGIC_VECTOR (arg));
  end function or_reduce;

  function or_reduce (arg : SIGNED)
    return STD_ULOGIC is
  begin
    return or_reduce (STD_ULOGIC_VECTOR (arg));
  end function or_reduce;

  function or_reduce (arg : STD_LOGIC_VECTOR)
    return STD_ULOGIC is
  begin
    return or_reduce (STD_ULOGIC_VECTOR (arg));
  end function or_reduce;

  -- http://www.eda.org/fphdl/std_logic_1164_additions.vhdl
  function TO_01 (s : double; xmap : STD_ULOGIC := '0')
    return double is
    variable RESULT      : double;
    variable BAD_ELEMENT : BOOLEAN := false;
  begin
    for I in RESULT'range loop
      case s(I) is
        when '0' | 'L' => RESULT(I)   := '0';
        when '1' | 'H' => RESULT(I)   := '1';
        when others    => BAD_ELEMENT := true;
      end case;
    end loop;
    if BAD_ELEMENT then
      for I in RESULT'range loop
        RESULT(I) := XMAP;              -- standard fixup
      end loop;
    end if;
    return RESULT;
  end function TO_01;

  -- Returns the class which X falls into
  function Classfp (
    x           : double;     -- floating point input
    check_error : BOOLEAN := true)   -- check for errors
    return valid_fpstate is
    constant fraction_width : INTEGER := 52;  -- length of FP output fraction
    constant exponent_width : INTEGER := 11;  -- length of FP output exponent
    variable arg            : double;
  begin  -- classfp
    if (arg'length < 1 or fraction_width < 3 or exponent_width < 3
        or x'left < x'right) then
      report "float_pkg:"
        & "CLASSFP: " &
        "Floating point number detected with a bad range"
        severity error;
      return isx;
    end if;
    -- Check for "X".
    arg := to_01 (x, 'X');
    if (arg(fraction_width) = 'X' or arg(0) = 'X') then
      return isx;                       -- If there is an X in the number
      -- Special cases, check for illegal number
    elsif check_error and
      (and_reduce (STD_ULOGIC_VECTOR (arg (exponent_width-1 + fraction_width downto fraction_width)))
       = '1') then                      -- Exponent is all "1".
      if or_reduce ( arg (fraction_width-1 downto 0))
        /= '0' then  -- Fraction must be all "0" or this is not a number.
        if (arg(fraction_width-1) = '1') then         -- From "W. Khan - IEEE standard
          return nan;            -- 754 binary FP Signaling nan (Not a number)
        else
          return quiet_nan;
        end if;
        -- Check for infinity
      elsif arg(exponent_width+fraction_width) = '0' then
        return pos_inf;                 -- Positive infinity
      else
        return neg_inf;                 -- Negative infinity
      end if;
      -- check for "0"
    elsif or_reduce (STD_LOGIC_VECTOR (arg (exponent_width-1+fraction_width downto fraction_width)))
      = '0' then                        -- Exponent is all "0"
      if or_reduce (arg (fraction_width-1 downto 0))
        = '0' then                      -- Fraction is all "0"
        if arg(exponent_width+fraction_width) = '0' then
          return pos_zero;              -- Zero
        else
          return neg_zero;
        end if;
      else
        if arg(exponent_width+fraction_width) = '0' then
          return pos_denormal;          -- Denormal number (ieee extended fp)
        else
          return neg_denormal;
        end if;
      end if;
    else
      if arg(exponent_width+fraction_width) = '0' then
        return pos_normal;              -- Normal FP number
      else
        return neg_normal;
      end if;
    end if;
  end function Classfp;

  -- to_real (float)
  -- typically not Synthesizable unless the input is a constant.
  function to_real (
    arg                  : double;        -- floating point input
    constant check_error : BOOLEAN    := true;  -- check for errors
    constant denormalize : BOOLEAN    := true)  -- Use IEEE extended FP
    return REAL is
    constant fraction_width : INTEGER := 52;  -- length of FP output fraction
    constant exponent_width : INTEGER := 11;  -- length of FP output exponent
    variable sign           : REAL;     -- Sign, + or - 1
    variable exp            : INTEGER;  -- Exponent
    variable expon_base     : INTEGER;  -- exponent offset
    variable frac           : REAL    := 0.0;       -- Fraction
    variable validfp        : valid_fpstate;        -- Valid FP state
    variable expon          : UNSIGNED (exponent_width - 1 downto 0)
      := (others => '1');               -- Vectorized exponent
  begin
    validfp := classfp (arg, check_error);
    classcase : case validfp is
      when isx | pos_zero | neg_zero | nan | quiet_nan =>
        return 0.0;
      when neg_inf =>
        return REAL'low;                -- Negative infinity.
      when pos_inf =>
        return REAL'high;               -- Positive infinity
      when others =>
        expon_base := 2**(exponent_width-1) -1;
        if to_X01(arg(exponent_width+fraction_width)) = '0' then
          sign := 1.0;
        else
          sign := -1.0;
        end if;
        -- Figure out the fraction
        for i in 0 to fraction_width-1 loop
          if to_X01(arg (fraction_width-1 - i)) = '1' then
            frac := frac + (2.0 **(-1 - i));
          end if;
        end loop;  -- i
        if validfp = pos_normal or validfp = neg_normal or not denormalize then
          -- exponent /= '0', normal floating point
          expon                   := UNSIGNED(arg (exponent_width-1+fraction_width downto fraction_width));
          expon(exponent_width-1) := not expon(exponent_width-1);
          exp                     := to_integer (SIGNED(expon)) +1;
          sign                    := sign * (2.0 ** exp) * (1.0 + frac);
        else  -- exponent = '0', IEEE extended floating point
          exp  := 1 - expon_base;
          sign := sign * (2.0 ** exp) * frac;
        end if;
        return sign;
    end case classcase;
  end function to_real;
end package body float_pkg_c_min;
