import pprint
import unittest

try:
    import tappy
    tappy_available = True
except ImportError:
    tappy_available = False

class PPrintTests(unittest.TestCase):
	def test_pprint_builder(self):
		builder = pprint.PrettyPrintBuilder()

		(builder.columns()
				.append()
					.add("blub")
					.add("xyz")
					.up()
				.append()
					.add("g")
					.add("hijk")
					.up()
				.up())
		pprinted = builder.build()

		self.assertEqual(str(pprinted), "blubg\nxyz hijk")

if __name__ == '__main__':
    if tappy_available:
        tappy.unittest_main(tapfile="python-ctemplate.tap")
    else:
        unittest.main()
