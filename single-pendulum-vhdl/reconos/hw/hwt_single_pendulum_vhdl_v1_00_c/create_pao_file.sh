#!/bin/bash

cd "$(dirname "$0")"

# symlink generated ipcores into hwt_single_pendulum_vhdl_v1_00_c library and add them to the *.pao file
DATA_DIR="data"
PAO_FILE="$DATA_DIR/hwt_single_pendulum_vhdl_v2_1_0.pao"
cat "$PAO_FILE.tpl" >"$PAO_FILE"
for vhd_file in ../../../ipcore_dir/*.vhd ; do
	ln -sf "../../$vhd_file" hdl/vhdl/

	name="$(basename "$vhd_file")"
	echo "lib hwt_single_pendulum_vhdl_v1_00_c ${name%%.vhd} vhdl" >>"$PAO_FILE"
done
