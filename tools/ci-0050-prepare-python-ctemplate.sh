#!/bin/sh

cd python-ctemplate

python setup.py build

BUILD_DIR="$(cd "$(dirname "$0")"/build/lib.* ; pwd)"

if [ ! -e "$BUILD_DIR" ] ; then
	echo "ERROR: I couldn't find the build directory to add it to the PYTHONPATH." >&2
	exit 1
fi

echo "export PYTHONPATH='$BUILD_DIR:$PYTHONPATH'" >>"$ENVVAR_STORE"
