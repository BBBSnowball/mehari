#!/bin/bash

if [ "$1" != "run-by-cmake" -o -z "$RECONOS" ] ; then
	echo "You shouldn't run this script directly." >&2
	exit 1
fi
shift

set -e

SOURCE_DIR="$(dirname "$0")"

. "$RECONOS/pcores/reconos_test_v3_01_a/testlib.sh"

check_environment

# determine which tests we should run
if [ -z "$1" -o "$1" == "*" -o "$1" == "work.all" ] ; then
	TESTS="ReturnValueReconOSTest GlobalArrayTest DoubleCommunicationTest IntCommunicationTest"
else
	TESTS="$*"
fi

sed 's~SOURCE_DIR~'"$SOURCE_DIR"'~' <"$SOURCE_DIR/project.prj" >project.prj

ReturnValueReconOSTest() {
	createProjectFile $test.prj           \
		project.prj                       \
		CodeGen_data/ReturnValueTest.vhdl \
		ReturnValueReconOS.vhdl           \
		"$SOURCE_DIR/unittests/CodeGen/ReturnValueReconOSTest.vhd" \
		"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
	runTest $test.prj "work.$test"
}

GlobalArrayTest() {
	createProjectFile $test.prj           \
		project.prj                       \
		$test/calculation.vhdl            \
		$test/reconos.vhdl                \
		CodeGen_data/${test}ReconOS.vhdl  \
		"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
	runTest $test.prj "work.$test"
}

DoubleCommunicationTest() {
	createProjectFile $test.prj           \
		project.prj                       \
		$test/calculation.vhdl            \
		$test/reconos.vhdl                \
		CodeGen_data/${test}ReconOS.vhdl  \
		"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
	runTest $test.prj "work.$test"
}

IntCommunicationTest() {
	createProjectFile $test.prj           \
		project.prj                       \
		$test/calculation.vhdl            \
		$test/reconos.vhdl                \
		CodeGen_data/${test}ReconOS.vhdl  \
		"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
	runTest $test.prj "work.$test"
}

BoolCommunicationTest() {
	createProjectFile $test.prj           \
		project.prj                       \
		$test/calculation.vhdl            \
		$test/reconos.vhdl                \
		CodeGen_data/${test}ReconOS.vhdl  \
		"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
	runTest $test.prj "work.$test"
}

for test in $TESTS ; do
	$test
done

echo "Tests: $TESTS"
echo "PASSED"
