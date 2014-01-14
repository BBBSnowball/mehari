#!/bin/sh

./check-deps-ci.sh

find -name test-ci.sh -execdir {} \;
