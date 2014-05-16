#!/bin/bash

NAME="$1"

set -e

cd "$(dirname "$0")"

load_bitstream() {
	if [ ! -f "$1" ] ; then
		echo "ERROR: Bitstream file '$1' doesn't exist!" >&2
		exit 1
	fi
	if [ ! -e "/dev/xdevcfg" ] ; then
		echo "ERROR: Device file /dev/xdevcfg doesn't exist!" >&2
		exit 1
	fi

	echo -n "Loading bitstream onto FPGA... "
	cat "$1" >/dev/xdevcfg

	if [ "$(cat /sys/devices/amba.0/f8007000.ps7-dev-cfg/prog_done)" -ne "1" ] ; then
		echo "ERROR: Couldn't download bitstream!" >&2
		exit 1
	fi

	echo "done"
}

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

PREFIX="${NAME}_"
RESULT_DIR="${NAME}_results"

#load_bitstream "$NAME.bin"

EXECUTABLE="../$NAME"
rm -rf "$RESULT_DIR"
mkdir -p "$RESULT_DIR"

( cd "$RESULT_DIR" && source "../${NAME}_measurements.sh" ) || exit $?

tar -cf "${PREFIX}results.tar" "$RESULT_DIR"
