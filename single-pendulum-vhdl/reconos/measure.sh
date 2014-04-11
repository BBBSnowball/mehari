#!/bin/sh

measure() {
	output="$1"
	count="$2"
	shift ; shift

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

measure "${PREFIX}hw_10k.txt"           10  ./single_pendulum_simple 1 0 -n 10000
measure "${PREFIX}sw_10k.txt"           10  ./single_pendulum_simple 0 1 -n 10000
measure "${PREFIX}hw_10k_wo_memory.txt" 10  ./single_pendulum_simple 1 0 -n 10000 --without-memory
measure "${PREFIX}sw_10k_wo_mbox.txt"   10  ./single_pendulum_simple 0 1 --software-thread-iterations 10000
