##############################################################################
## Filename:          hwt_single_pendulum_simple_v2_1_0.pao
## Description:       Simple implementation of Single Pendulum (not compatible with Ipanema)
## Date:              Tue Sep 13 12:47:26 2011 (by Create and Import Peripheral Wizard)
##############################################################################

lib proc_common_v3_00_a  proc_common_pkg
lib reconos_v3_01_a reconos_pkg

# single pendulum and ReconOS wrapper
lib hwt_single_pendulum_simple_v1_00_c single_pendulum vhdl
lib hwt_single_pendulum_simple_v1_00_c hwt_single_pendulum_simple vhdl

# helpers
lib hwt_single_pendulum_simple_v1_00_c double_type vhdl
lib hwt_single_pendulum_simple_v1_00_c float_helpers vhdl
lib hwt_single_pendulum_simple_v1_00_c float_pkg_c_min vhdl

# custom arithmetic cores
lib hwt_single_pendulum_simple_v1_00_c float_neg vhdl
lib hwt_single_pendulum_simple_v1_00_c float_sin vhdl
lib hwt_single_pendulum_simple_v1_00_c float_cos vhdl

# arithmetic cores by Xilinx (generated with coregen)
# (added by test.sh script)
