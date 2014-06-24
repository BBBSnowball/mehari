#!/bin/bash

set -e

cd "$(dirname "$0")"

ln -sf ../../float_library_v1_00_a/data/float_library_v2_1_0.bbd data/hwt_single_pendulum_simple_v2_1_0.bbd
ln -sf ../float_library_v1_00_a/netlist .
