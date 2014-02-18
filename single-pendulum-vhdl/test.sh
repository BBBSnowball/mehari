#!/bin/bash

set -e

cd "$(dirname "$0")"

# generate tests
LD_LIBRARY_PATH= ./generate_tests.py

# generate data for test_float_conversion.vhd
python float-testdata.py


RECONOS=../reconos/reconos
. "$RECONOS/pcores/reconos_test_v3_01_a/testlib.sh"

check_environment

# determine which tests we should run
if [ -z "$1" -o "$1" == "*" -o "$1" == "work.all" ] ; then
	TESTS=""
	for test in test_*.vhd test_gen/test_*.vhd ; do
		if [ "$test" != "test_helpers.vhd" ] ; then
			TESTS="$TESTS work.$(basename $test .vhd)"
		fi
	done
else
	TESTS="$*"
fi

createProjectFile all.prj \
	project.prj           \
	test_gen/tests.prj    \
	"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"

for test in $TESTS ; do
	runTest all.prj "$test"
done

echo "Tests: $TESTS"
echo "PASSED"
