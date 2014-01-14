#!/bin/sh

./check-ci-deps.sh

find -name test-ci.sh -execdir {} \;
