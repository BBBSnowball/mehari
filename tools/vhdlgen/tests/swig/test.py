import unittest
from test_pprint import *
from test_vhdlgen import *

try:
    import tappy
    tappy_available = True
except ImportError:
    tappy_available = False

import os
print("PYTHONPATH: " + os.environ["PYTHONPATH"])

if __name__ == '__main__':
    if tappy_available:
        tappy.unittest_main(tapfile="python-pprint-and-vhdlgen.tap")
    else:
        unittest.main()
