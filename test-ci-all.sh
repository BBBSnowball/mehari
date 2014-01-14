#!/bin/sh

set -e

./check-deps-ci.sh

find -name test-ci.sh -exec echo -e "\n\n\n===== Running {} =====\n\n" \; -execdir {} \;
