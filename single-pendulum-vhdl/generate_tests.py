#!/usr/bin/env python

import yaml, ctemplate, os.path, re

with open("operations.yaml") as f:
	operations_db = yaml.load(f)

#TODO this doesn't work :-(
ctemplate.AddModifier("x-fix-indent=", lambda text,indent: text.replace("\n", "\n"+indent[1:]))

test_unary_real_function = ctemplate.Template("test_unary_real_function.vhd.tpl", ctemplate.DO_NOT_STRIP)

test_dir = "test_gen"
if not os.path.exists(test_dir):
    os.makedirs(test_dir)

def get(dictionary, *keys, **kwargs):
	for key in keys:
		if key in dictionary:
			return dictionary[key]

	if "default" in kwargs:
		return kwargs["default"]
	else:
		raise KeyError(" or ".join(map(repr, keys)))

with open(os.path.join(test_dir, "tests.prj"), "w") as prj:
	for operation in operations_db["operations"]:
		print("Generating test for %r..." % operation["signature"])

		test_filename = "test_"  + operation["vhdl-component"] + ".vhd"

		data = ctemplate.Dictionary(test_filename)
		data["source_files"]     = "operations.yaml and test_unary_real_function.vhd.tpl"
		data["uut_name"]         = operation["vhdl-component"]
		data["input_a_name"]     = get(operation, "argument_names",   default="input_a")
		data["result_name"]      = get(operation, "result_name",      default="result" )
		data["max_clock_cycles"] = get(operation, "max-clock-cycles", default="1000"   )

		signature = operation["signature"]
		result_type = "\s*->\s*\w+$"
		m_function = re.match("^(\w+)\((.*)\)" + result_type, signature)
		m_prefix_op = re.match("^(\W+)\s*\w+"  + result_type, signature)
		if m_function:
			arg_types = m_function.group(2).split(",")
			expected_value_calculation = m_function.group(1) + "(input_a_value)"
		elif m_prefix_op:
			expected_value_calculation = m_prefix_op.group(1) + "input_a_value"
		else:
			raise NotImplementedError("Cannot parse signature: %r" % (signature,))

		data["expected_value_calculation"] = expected_value_calculation

		testset_name = operation["tests"]
		testset = operations_db["tests"][testset_name]
		for test in testset:
			test_data = data.AddSectionDictionary("TESTS")
			test_data["TEST"] = test

		with open(os.path.join(test_dir, test_filename), "w") as f:
			f.write(test_unary_real_function.Expand(data))

		prj.write("vhdl work \"%s\"\n" % (os.path.join(test_dir, test_filename),))
