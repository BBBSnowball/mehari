#!/bin/bash

# This file runs all script for continuous integration. The order is like this:
# 1. ci-NNNN-name.sh - sorted by filename, NNNN is a 4-digit number, name is a description (without spaces)
# 2. test-ci.sh or test-ci-name.sh - sorted by whole path, but order shouldn't matter
#
# The number part in ci-NNNN-name.sh has this informal meaning:
# 0000 - 0049: check dependencies
# 0050 - 0099: build tools that we need for the build system (e.g. python-ctemplate)
# 0200 - 0299: clean
# 0400 - 0499: build mehari
# 0600 - 0699: build tests
# 0800 - 0899: package build results and/or install to temporary location
# 0900 - 0990: final preparations for the tests

set -e

cd "$(dirname "$0")"

# scripts will store export statements in this file and we will include it, so the variables
# are available for all further scripts
export ENVVAR_STORE="$(pwd)/.tmp-envvar_store"
[ -e "$ENVVAR_STORE" ] && rm "$ENVVAR_STORE"
ENVVAR_STORE_ALL=".tmp-envvar_store-all"
[ -e "$ENVVAR_STORE_ALL" ] && rm "$ENVVAR_STORE_ALL"
touch "$ENVVAR_STORE_ALL"
TASK_LIST=".tmp-tasks"

echo "Gathering task list..."

find -regex ".*/ci-[0-9]+-[^/ ]+\.[^/ ]+" | awk -F "/" '{print $NF " " $0}' \
	| sort \
	| cut -d " " -f2- \
	>"$TASK_LIST"
if ! (($DONT_TEST)) ; then
	find -name test-ci*.sh | sort >>"$TASK_LIST"
fi
TASK_COUNT="$(wc -l "$TASK_LIST" | cut -d " " -f1)"
echo "Found $TASK_COUNT tasks."

TASK_NUM=0
while read script ; do
	TASK_NUM=$[$TASK_NUM+1]
	echo -e "\n\n\n===== Running $script ($TASK_NUM / $TASK_COUNT) =====\n\n"

	if ! [ -x "$script" ] ; then
		echo "WARNING: $script is not executable. I will fix that for you." >&2
		chmod +x "$script"
	fi

	( cd "$(dirname "$script")" && "./$(basename "$script")" ) || (
		EXITCODE=$?
		echo -e "\n\nFailed. Exit code is $EXITCODE.\n\n" >&2
		exit $EXITCODE
	)

	if [ ! -e "$ENVVAR_STORE_ALL" -o ! -e "$TASK_LIST" ] ; then
		echo "\n\nERROR: $ENVVAR_STORE_ALL and/ or $TASK_LIST have been deleted!" >&2
		exit 1
	fi

	[ -e "$ENVVAR_STORE" ] \
		&& cat "$ENVVAR_STORE" \
		&& cat "$ENVVAR_STORE" >>"$ENVVAR_STORE_ALL" \
		&& source "$ENVVAR_STORE" \
		&& rm "$ENVVAR_STORE"
done <"$TASK_LIST"

echo
echo
echo "all done"
