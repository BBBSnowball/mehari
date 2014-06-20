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
	TESTS=""
	for test in *.vhdl ; do
		if [ "$(basename $test .vhdl)" != "ReturnValueReconOS" ] ; then
			TESTS="$TESTS $(basename $test .vhdl)"
		fi
	done
else
	TESTS="$*"
fi

sed 's~SOURCE_DIR~'"$SOURCE_DIR"'~' <"$SOURCE_DIR/project.prj" >project.prj

for test in $TESTS ; do
	createProjectFile $test.prj   \
		project.prj               \
		CodeGen_data/$test.vhdl   \
		$test.vhdl                \
		"$RECONOS/pcores/reconos_test_v3_01_a/hdl/vhdl/test_helpers.vhd"
	runTest $test.prj "work.$test"
done

echo "Tests: $TESTS"
echo "PASSED"
