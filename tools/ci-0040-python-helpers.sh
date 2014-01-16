#!/bin/sh

PYTHON_HELPERS_DIR="$(cd python-helpers ; pwd)"

echo "export PYTHONPATH='$PYTHON_HELPERS_DIR:'\"\$PYTHONPATH\"" >>"$ENVVAR_STORE"
