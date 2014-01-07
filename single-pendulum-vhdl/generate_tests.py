#!/usr/bin/env python

import yaml, ctemplate, os.path, re

with open("operations.yaml") as f:
	operations_db = yaml.load(f)

ctemplate.AddModifier("x-fix-indent=", lambda text,indent: text.replace("\n", "\n"+indent[1:]))

def load_template(filename):
	tpl = ctemplate.Template(filename, ctemplate.DO_NOT_STRIP)
	return (tpl, filename)

test_unary_real_function = load_template("test_unary_real_function.vhd.tpl")
test_binary_real_function = load_template("test_binary_real_function.vhd.tpl")

def select_test_template(arg_types, result_type):
	if arg_types == ["real"] and result_type == "real":
		return test_unary_real_function
	elif arg_types == ["real", "real"] and result_type == "real":
		return test_binary_real_function
	else:
		raise NotImplementedError("I don't have a template that supports the types %r -> %r. Sorry."
			% (arg_types, result_type))

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

def inputname_vars():
	for index in range(26):
		yield "input_%c_name" % chr(ord('a') + index)

def default_input_names():
	for index in range(26):
		yield "input_%c" % chr(ord('a') + index)

with open(os.path.join(test_dir, "tests.prj"), "w") as prj:
	for operation in operations_db["operations"]:
		print("Generating test for %r..." % operation["signature"])

		test_filename = "test_"  + operation["vhdl-component"] + ".vhd"

		data = ctemplate.Dictionary(test_filename)
		data["uut_name"]         = operation["vhdl-component"]
		data["result_name"]      = get(operation, "result_name",      default="result" )
		data["max_clock_cycles"] = get(operation, "max-clock-cycles", default="1000"   )

		signature = operation["signature"]
		result_type_pattern = "\s*->\s*(\w+)$"
		m_function = re.match("^(\w+)\((.*)\)" + result_type_pattern, signature)
		m_prefix_op = re.match("^(\W+)\s*(\w+)"  + result_type_pattern, signature)
		m_binary_op = re.match("^(\w+)\s*(\W+)\s*(\w+)" + result_type_pattern, signature)
		if m_function:
			arg_types = m_function.group(2).split(",")
			result_type = m_function.group(3)
			expected_value_calculation = m_function.group(1) + "(input_a_value)"
		elif m_prefix_op:
			arg_types = [m_prefix_op.group(2)]
			result_type = m_prefix_op.group(3)
			expected_value_calculation = m_prefix_op.group(1) + "input_a_value"
		elif m_binary_op:
			arg_types = [m_binary_op.group(1), m_binary_op.group(3)]
			result_type = m_binary_op.group(4)
			expected_value_calculation = "input_a_value " + m_binary_op.group(2).strip() + " input_b_value"
		else:
			raise NotImplementedError("Cannot parse signature: %r" % (signature,))

		data["expected_value_calculation"] = expected_value_calculation

		arg_names = get(operation, "argument_names", default=",".join(default_input_names()))
		arg_names = arg_names.split(",")
		for varname, input_name in zip(inputname_vars(), arg_names):
			data[varname] = input_name.strip()

		testset_name = operation["tests"]
		testset = operations_db["tests"][testset_name]
		for test in testset:
			test_data = data.AddSectionDictionary("TESTS")
			test_data["TEST"] = test

		template, template_filename = select_test_template(arg_types, result_type)

		data["source_files"]     = " and ".join(["operations.yaml", template_filename])

		with open(os.path.join(test_dir, test_filename), "w") as f:
			f.write(template.Expand(data))

		prj.write("vhdl work \"%s\"\n" % (os.path.join(test_dir, test_filename),))
