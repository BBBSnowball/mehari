#!/bin/bash

set -e

cd "$(dirname "$0")"
BASEDIR="$(pwd)"

[ -n "$IPANEMA" ]  || IPANEMA=$BASEDIR/../private/ipanema
IPANEMA_INCLUDES_SYSLAYER=$IPANEMA/include/syslayer/linux
IPANEMA_INCLUDES_SIM=$IPANEMA/include/sim

[ -n "$LLVM" ]  || LLVM=$BASEDIR/../llvm
LLVM_BIN=$LLVM/Release+Asserts/bin

[ -n "$IR_DIR"    ]  || IR_DIR=$BASEDIR/ir
[ -n "$GRAPH_DIR" ]  || GRAPH_DIR=$BASEDIR/graph

CFLAGS="-DDOUBLE_FLOAT -D__POSIX__ -D_DSC_EVENTQUEUE -D__RESET -DEXEC_SEQ"


print_usage() {
	echo "usage: $0 [-v] [sourcefile]"
}

print_verbose() {
	if (($VERBOSE)) ; then
		echo $1
	fi
}

clean() {
	rm -rf $IR_DIR
	rm -rf $GRAPH_DIR
}


# command line flags
while getopts "vxh" opt
do
	case "$opt" in
	  v)
		VERBOSE=1
		;;
	  x)
		CLEAN=1
		;;
	  h)
		print_usage
		exit 0 
		;;
	 \?)
		echo "unknown flag: $opt"
		print_usage
		exit 1 
		;;
	esac
done
shift `expr $OPTIND - 1`


# command line argument
if [ -z "$1" ]; then
	echo "You must specify a valid source file to run this script."
	print_usage
	exit 1
fi
if [ ! -f "$1" ]; then
	echo "$1 is no valid source file."
	print_usage
	exit 1
fi

SOURCE_FILE="$(readlink -f $1)"

print_verbose "Apply LLVM optimizations to file: $SOURCE_FILE"


if (($CLEAN)) ; then
	print_verbose "Clean up"
	clean
fi
mkdir -p "$IR_DIR"
mkdir -p "$GRAPH_DIR"


create_ir() {
	local FILE=$1
	local resultvar=$2
	local OUTFILE="$IR_DIR/$(basename "${FILE%.*}").ll"
	print_verbose "Create intermediate representation for $FILE"
	$LLVM_BIN/clang "$CFLAGS" -S -emit-llvm -I"$IPANEMA_INCLUDES_SYSLAYER" -I"$IPANEMA_INCLUDES_SIM" "$FILE" -o "$OUTFILE"
	eval $resultvar="'$OUTFILE'"
}

create_callgraph() {
	local FILE=$1
	local FILENAME=$(basename "${FILE%.*}")
	print_verbose "Export callgraph for $FILE"
	$LLVM_BIN/opt -analyze -dot-callgraph "$FILE" > /dev/null
	dot -Tpdf callgraph.dot -o "$GRAPH_DIR/$FILENAME.pdf"
}

apply_inline_opt() {
	local FILE=$1
	local resultvar=$2
	local OUTFILE="$IR_DIR/$(basename "${FILE%.*}")-inline.ll"
	print_verbose "Apply inline optimization"
	$LLVM_BIN/opt -inline -S "$FILE" -o "$OUTFILE"
	eval $resultvar="'$OUTFILE'"
}


# main
ir_file=
create_ir "$SOURCE_FILE" ir_file
create_callgraph "$ir_file"

ir_file_inline=
apply_inline_opt "$ir_file" ir_file_inline
create_callgraph "$ir_file_inline"

# TODO write dot source to a specified file
rm callgraph.dot
