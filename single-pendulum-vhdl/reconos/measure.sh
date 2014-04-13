#!/bin/bash

cd "$(dirname "$0")"

measure() {
	output="$1"
	count="$2"
	shift ; shift

	echo "Command: $*" >"$output"
	echo >> "$output"

	echo -n "$*: "

	i=0
	while [ $i -lt "$count" ] ; do
		echo -n "."
		"$@" >>"$output"
		i=$((i+1))
	done
	echo " done"

	echo -n "Average calculation time: "
	extract_calculation_time "$output" | avg
}

extract_calculation_time() {
	sed -n 's/^\s*Calculation\s*:\s*\([0-9]\+\)\s*ms$/\1/p' "$1"
}

div_rounding() {
	echo $(( ($1+$2/2) / $2 ))
}

avg() {
	SUM=0
	COUNT=0
	while read x ; do
		if [ -n "$x" ] ; then
			SUM=$((SUM+x))
			COUNT=$((COUNT+1))
		fi
	done
	div_rounding $SUM $COUNT
}

PREFIX="single_pendulum_simple_"
ITERATIONS=10000

rm ${PREFIX}*.perf.txt

measure "${PREFIX}hw_10k.perf.txt"                   10  ./single_pendulum_simple 1 0 -n $ITERATIONS
measure "${PREFIX}sw_10k.perf.txt"                   10  ./single_pendulum_simple 0 1 -n $ITERATIONS
measure "${PREFIX}hw_10k_wo_memory.perf.txt"         10  ./single_pendulum_simple 1 0 -n $ITERATIONS --without-memory
measure "${PREFIX}sw_100k_wo_mbox.perf.txt"          10  ./single_pendulum_simple 0 1 -m $((10*ITERATIONS))
measure "${PREFIX}hw_100k_wo_mbox.perf.txt"          10  ./single_pendulum_simple 1 0 -m $((10*ITERATIONS))
measure "${PREFIX}hw_10k_noflush.perf.txt"           10  ./single_pendulum_simple 1 0 -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_10k_noflush.perf.txt"           10  ./single_pendulum_simple 0 1 -n $ITERATIONS --dont-flush
measure "${PREFIX}hw_10k_wo_memory_noflush.perf.txt" 10  ./single_pendulum_simple 1 0 -n $ITERATIONS --dont-flush --without-memory

tar -cf single_pendulum_simple_results.tar ${PREFIX}*.perf.txt
