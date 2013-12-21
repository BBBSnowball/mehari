LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.math_real.ALL;

package test_helpers is
  procedure assertAlmostEqual(actual : in real;
                              expected : in real;
                              epsilon : in real := 1.0e-10);

  procedure endOfSimulation(dummy : in integer := 0);

  -- http://www-ee.uta.edu/online/zhu/spring_2007/tutorial/how_to_print_objexts.txt
  function to_string(sv: Std_Logic_Vector) return string;

  procedure assertEqual(actual :   in std_logic_vector;
                        expected : in std_logic_vector);

  procedure assertEqual(actual : in integer;
                        expected : in integer);
end package test_helpers;

package body test_helpers is
  procedure assertEqual(actual :   in std_logic_vector;
                        expected : in std_logic_vector) is
  begin
    assert actual = expected
      report to_string(actual) & " /= " & to_string(expected);
  end procedure;

  procedure assertEqual(actual : in integer;
                        expected : in integer) is
  begin
    assert actual = expected
      report integer'image(actual) & " /= " & integer'image(expected);
  end procedure;

  procedure assertAlmostEqual(actual : in real;
                              expected : in real;
                              epsilon : in real := 1.0e-10) is    --TODO is this a good default value?
  begin
    assert abs(actual - expected) < epsilon
      report real'image(actual) & " /= " & real'image(expected)
        & ", difference is " & real'image(actual-expected);
  end procedure;

  procedure endOfSimulation(dummy : in integer := 0) is
  begin
    -- http://www.velocityreviews.com/forums/t57165-how-to-stop-simulation-in-vhdl.html
    assert false report " NONE. End of simulation." severity failure;
  end;

  -- http://www-ee.uta.edu/online/zhu/spring_2007/tutorial/how_to_print_objexts.txt
  function to_string(sv: Std_Logic_Vector) return string is
    use Std.TextIO.all;
    variable bv: bit_vector(sv'range) := to_bitvector(sv);
    variable lp: line;
  begin
    write(lp, bv);
    return lp.all;
  end;
end package body test_helpers;
