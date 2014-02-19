#!/bin/bash

set -e

cd "$(dirname "$0")"

[ -z "$RECONOS" ] && RECONOS=../../../reconos/reconos

. "$RECONOS/pcores/reconos_test_v3_01_a/testlib.sh"

check_environment

createProjectFile all.prj \
	"$RECONOS/pcores/reconos_v3_01_a/data/reconos_v2_1_0.pao" \
	"$RECONOS/pcores/reconos_test_v3_01_a/data/reconos_test_v2_1_0.pao" \
	"$XILINX/../EDK/hw/XilinxProcessorIPLib/pcores/proc_common_v3_00_a/data/proc_common_v2_1_0.pao" \
	"../hw/hwt_single_pendulum_vhdl_v1_00_c/data/hwt_single_pendulum_vhdl_v2_1_0.pao" \
	library:hwt_single_pendulum_vhdl_v1_00_c \
	"../../double_type.vhd" \
	"../../float_helpers.vhd" \
	"../../float_pkg_c_min.vhd" \
	"../../single_pendulum.vhd" \
	../../float_{neg,sin,cos}.vhd \
	../../ipcore_dir/*.vhd \
	library:work \
	"single_pendulum_simple_test.vhd"

runTest all.prj work.single_pendulum_simple_test || exit $?

echo "PASSED"
