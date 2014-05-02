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

PREFIX="mbox_put_get_"
ITERATIONS=10000

rm ${PREFIX}*.perf.txt

measure "${PREFIX}hw_1_10k.perf.txt" 10   ./mbox_put_get 1 0  -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_1_10k.perf.txt" 10   ./mbox_put_get 0 1  -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_2_10k.perf.txt" 10   ./mbox_put_get 0 2  -n $ITERATIONS --dont-flush
measure "${PREFIX}sw_32_10k.perf.txt" 10  ./mbox_put_get 0 32 -n $ITERATIONS --dont-flush

tar -cf mbox_put_get_results.tar ${PREFIX}*.perf.txt
