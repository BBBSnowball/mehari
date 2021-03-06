import struct
import math

# format of VHDL data files (as generated by Xilinx ISim):
# - real is "<d"
# - std_logic is "B", 0x02 for "0", 0x03 for "1"
# - std_logic_vector is many std_logic, left to right
# - record is just the contained data

std_logic_low = 0x02
std_logic_high = 0x03

test_values = [math.pi, -math.pi, 10000.7, 1.0]

with open("float-testdata.dat", "w") as f:
	for test_value in test_values:
		f.write(struct.pack("<d", test_value))

		bytes = struct.unpack("8B", struct.pack(">d", test_value))
		bits = []
		for byte in bytes:
			for i in xrange(7, -1, -1):
				if byte & (1<<i) != 0:
					bits.append(std_logic_high)
				else:
					bits.append(std_logic_low)
		f.write(struct.pack("64B", *bits))
