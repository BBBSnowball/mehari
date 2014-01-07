#!/bin/bash

set -e

# Important directories and settings
cd "$(dirname "$0")"
BASEDIR="$(pwd)"

[ -n "$IPANEMA" ]  || IPANEMA=$BASEDIR/../private/ipanema
IPANEMA_INCLUDES_SYSLAYER=$IPANEMA/include/syslayer/linux
IPANEMA_INCLUDES_SIM=$IPANEMA/include/sim

[ -n "$LLVM" ]  || LLVM=$BASEDIR/../llvm

LLVM_CONFIG_OPTIONS="--disable-optimized"
#LLVM_CONFIG_OPTIONS="--enable-optimized"

LLVM_BIN=$LLVM/Debug+Asserts/bin
LLVM_LIB=$LLVM/Debug+Asserts/lib

LLVM_CUSTOM_PASSES=$LLVM/lib/Transforms/Mehari/

[ -n "$IR_DIR"    ]  || IR_DIR=$BASEDIR/ir
[ -n "$GRAPH_DIR" ]  || GRAPH_DIR=$BASEDIR/graph

[ -n "$MAKE_PARALLEL_PROCESSES" ] || MAKE_PARALLEL_PROCESSES=4

CFLAGS="-DDOUBLE_FLOAT -D__POSIX__ -D_DSC_EVENTQUEUE -D__RESET -DEXEC_SEQ"


# some helper functions
print_usage() {
	echo "usage: $0 [-v] [-s] [-x] [sourcefile]"
	echo " -v  explain what is being done"
	echo " -s  skip building LLVM"
	echo " -x  clean up results from last run"
}

print_verbose() {
	if (($VERBOSE)) ; then
		echo " ### $1"
	fi
}


# command line flags
while getopts "vsxh" opt
do
	case "$opt" in
	  v)
		VERBOSE=1
		;;
	  s)
		SKIP_BUILDING_LLVM=1
		;;
	  x)
		CLEAN_RESULTS=1
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
print_verbose "---------------------------------"


# 1. clean repositories, results and llvm
# ---------------------------------------
clean_results() {
	cd $BASEDIR
	rm -rf $IR_DIR
	rm -rf $GRAPH_DIR
}

clean_repository() {
	( cd "$1" && git reset --hard && git clean -f -x -d )
}

# TODO is it necessary to use this if the git is cleaned as well?
clean_llvm() {
	cd $LLVM
	gmake dist-clean
}


if (($CLEAN_RESULTS)) ; then
	print_verbose "Clean up results"
	clean_results
fi
mkdir -p "$IR_DIR"
mkdir -p "$GRAPH_DIR"


if ! (($SKIP_BUILDING_LLVM)) ; then
	print_verbose "Clean git repositories"
	clean_repository "$LLVM"
	# TODO is it necessary to clean the submodules as well?
	clean_repository "$LLVM/tools/clang"
	clean_repository "$LLVM/projects/compiler-rt"
	clean_repository "$LLVM/projects/test-suite"
fi


# 2. configure and build llvm 
# ---------------------------
build_llvm() {
	cd $LLVM
	$LLVM/configure $LLVM_CONFIG_OPTIONS
	gmake -j"$MAKE_PARALLEL_PROCESSES"
}

if ! (($SKIP_BUILDING_LLVM)) ; then
	build_llvm
fi


# 3. build all custom optimizations 
# ---------------------------------
cd $LLVM_CUSTOM_PASSES
for dir in "$LLVM_CUSTOM_PASSES"* ; do
	cd $dir
	gmake
done


# 4. run example
# --------------
create_ir() {
	local FILE="$1"
	local resultvar="$2"
	local OUTFILE="$IR_DIR/$(basename "${FILE%.*}").ll"
	print_verbose "Create intermediate representation for $FILE"
	$LLVM_BIN/clang "$CFLAGS" -S -emit-llvm -I"$IPANEMA_INCLUDES_SYSLAYER" -I"$IPANEMA_INCLUDES_SIM" "$FILE" -o "$OUTFILE"
	eval $resultvar="'$OUTFILE'"
}

create_callgraph() {
	local FILE="$1"
	local FILENAME=$(basename "${FILE%.*}")
	print_verbose "Export callgraph for $FILE"
	$LLVM_BIN/opt -analyze -dot-callgraph "$FILE" > /dev/null
	dot -Tpdf callgraph.dot -o "$GRAPH_DIR/$FILENAME.pdf"
}

prepare_inline_opt() {
	local FILE="$1"
	local resultvar="$2"
	local OUTFILE="$IR_DIR/$(basename "${FILE%.*}")-pre-inline.ll"
	print_verbose "Prepare inline optimization"
	$LLVM_BIN/opt -load "$LLVM_LIB/LLVMPrepareAlwaysInline.so" -prepare-always-inline -S "$FILE" -o "$OUTFILE"
	eval $resultvar="'$OUTFILE'"
}

apply_inline_opt() {
	local FILE="$1"
	local resultvar="$2"
	local OUTFILE="$IR_DIR/$(basename "${FILE%.*}")-inline.ll"
	print_verbose "Apply inline optimization"
	$LLVM_BIN/opt -always-inline -S "$FILE" -o "$OUTFILE"
	eval $resultvar="'$OUTFILE'"
}


cd $BASEDIR

# first create intermediate representation for source file
ir_file=
create_ir "$SOURCE_FILE" ir_file
create_callgraph "$ir_file"

# prepare inline optimization
ir_file_pre_inline=
prepare_inline_opt "$ir_file" ir_file_pre_inline

# run inline optimization
ir_file_inline=
apply_inline_opt "$ir_file_pre_inline" ir_file_inline
create_callgraph "$ir_file_inline"

# run custom optimization passes
$LLVM_BIN/opt -load "$LLVM_LIB/LLVMFirstPass.so" -firstpass -S "$ir_file_inline" > /dev/null

# TODO write dot source to a specified file
rm callgraph.dot
