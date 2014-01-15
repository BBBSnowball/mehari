#!/bin/bash

set -e

cd "$(dirname "$0")"

# scripts will store export statements in this file and we will include it, so the variables
# are available for all further scripts
export ENVVAR_STORE="$(pwd)/.tmp-envvar_store"
[ -e "$ENVVAR_STORE" ] && rm "$ENVVAR_STORE"

echo "Gathering task list..."

find -name test-ci.sh -or -regex ".*/ci-[0-9]+-[^/ ]+\.[^/ ]+" | awk -F "/" '{print $NF " " $0}' \
	| sort \
	| cut -d " " -f2- \
	>.tmp-tasks
TASK_COUNT="$(wc -l .tmp-tasks | cut -d " " -f1)"
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

	[ -e "$ENVVAR_STORE" ] && cat "$ENVVAR_STORE" && source "$ENVVAR_STORE" && rm "$ENVVAR_STORE"
done <.tmp-tasks

echo "all done"
