#!/bin/bash

set -e

cd "$(dirname "$0")"

LD_LIBRARY_PATH= ./generate_tests.py
cat all.prj test_gen/tests.prj >all2.prj

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

# The Xilinx settings script is confused by our arguments, so we must remove them.
while [ -n "$1" ] ; do
	shift
done

[ -n "$XILINX_SETTINGS_SCRIPT" ] && source "$XILINX_SETTINGS_SCRIPT"


cat >run_test.tcl <<EOF
cd "$(realpath "$(dirname "$0")")"
run all
exit
EOF

for test in $TESTS ; do
	[ -e "fuse.log" ] && rm fuse.log
	fuse -incremental -prj all2.prj -o test_sim $test || exit $?

	if ! [ -e "fuse.log" ] ; then
		echo "ERROR: Fuse hasn't created logfile isim.log!" >&2
		exit 1
	fi

	if grep "^WARNING:.* remains a black-box since it has no binding entity.$" fuse.log >&2 ; then
		echo "ERROR: Simulation will not work, if an entity is missing. Please add it to all.prj" >&2
		exit 1
	fi

	[ -e "isim.log" ] && rm isim.log
	./test_sim -intstyle ise -tclbatch run_test.tcl || exit $?

	if ! [ -e "isim.log" ] ; then
		echo "ERROR: ISim hasn't created logfile isim.log!" >&2
		exit 1
	fi

	if ! grep -q "Simulator is doing circuit initialization process" isim.log ; then
		# We could also check for "Finished circuit initialization process", but it seems that
		# this is not printed, if we abort the simulation.
		echo "There seams to be some problem with the simulation." >&2
		exit 1
	fi

	if grep -q "The simulator has terminated in an unexpected manner" isim.log ; then
		echo "There seams to be some problem with the simulation." >&2
		exit 1
	fi

	if ! grep -q "INFO: Simulator is stopped" isim.log ; then
		echo "There seams to be some problem with the simulation." >&2
		exit 1
	fi

	if grep -v "^\*\* Failure:\s*NONE\. End of simulation\." isim.log | grep -q "^\*\* Failure:" ; then
		echo "The simulation has been stopped by a fatal error!" >&2
		exit 1
	fi

	# We use grep without '-q', so the user will see the error messages again.
	if grep "^at [^:]*: Error: " isim.log >&2 || grep -i "^Error: " isim.log >&2 ; then
		echo "There was at least one error during the test run!" >&2
		exit 1
	fi
done

echo "Tests: $TESTS"
echo "PASSED"
