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
TESTS="ReturnValueReconOSTest"

sed 's~SOURCE_DIR~'"$SOURCE_DIR"'~' <"$SOURCE_DIR/project.prj" >project.prj

test=ReturnValueReconOSTest
createProjectFile $test.prj           \
	project.prj                       \
	CodeGen_data/ReturnValueTest.vhdl \
	ReturnValueReconOS.vhdl           \
	"$SOURCE_DIR/unittests/CodeGen/ReturnValueReconOSTest.vhd" \
	"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
runTest $test.prj "work.$test"

echo "Tests: $TESTS"
echo "PASSED"
