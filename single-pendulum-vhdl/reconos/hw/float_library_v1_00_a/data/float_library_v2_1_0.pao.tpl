##############################################################################
## Filename:          float_library_v1_1_0.pao
## Description:       Floating point cores and libraries
## Date:              Tue Sep 13 12:47:26 2011 (by Create and Import Peripheral Wizard)
##############################################################################

lib proc_common_v3_00_a  proc_common_pkg
lib reconos_v3_01_a reconos_pkg

# helpers
lib float_library_v1_00_a double_type vhdl
lib float_library_v1_00_a float_helpers vhdl
lib float_library_v1_00_a float_pkg_c_min vhdl

# custom arithmetic cores
lib float_library_v1_00_a float_neg vhdl
lib float_library_v1_00_a float_sin vhdl
lib float_library_v1_00_a float_cos vhdl

# other cores
lib float_library_v1_00_a add vhdl
lib float_library_v1_00_a bit_or vhdl
lib float_library_v1_00_a dummy_mod vhdl
lib float_library_v1_00_a icmp_ne vhdl
lib float_library_v1_00_a icmp_sgt vhdl

# arithmetic cores by Xilinx (generated with coregen)
# (added by test.sh script)
