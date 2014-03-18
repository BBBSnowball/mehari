import vhdlgen
import unittest
import re

try:
    import tappy
    tappy_available = True
except ImportError:
    tappy_available = False

class VhdlgenTests(unittest.TestCase):
	def test_pprint_builder(self):
		file = vhdlgen.CompilationUnit()

		file.add(vhdlgen.Architecture("behavior", "bla"))

		file.libraries().add("ieee")   \
			.use("std_logic_1164.all") \
			.use("numeric_std.all")

		self.assertEqual(str(file.prettyPrint()),
			re.sub("(?m)\t{3}", "", """library ieee;
			use ieee.numeric_std.all;
			use ieee.std_logic_1164.all;


			architecture behavior of bla is
			begin
			end behavior;"""))


if __name__ == '__main__':
    if tappy_available:
        tappy.unittest_main(tapfile="python-ctemplate.tap")
    else:
        unittest.main()
