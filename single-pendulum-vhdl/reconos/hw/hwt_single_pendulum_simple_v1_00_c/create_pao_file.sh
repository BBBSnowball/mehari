#!/bin/bash

set -e

cd "$(dirname "$0")"

library="$(basename "$(pwd)")"

# symlink generated ipcores into hwt_single_pendulum_simple_v1_00_c library and add them to the *.pao file
DATA_DIR="data"
PAO_FILE="$DATA_DIR/hwt_single_pendulum_simple_v2_1_0.pao"
cat "$PAO_FILE.tpl" >"$PAO_FILE"
for vhd_file in ../../../ipcore_dir/*.vhd ; do
	ln -sf "../../$vhd_file" hdl/vhdl/

	name="$(basename "$vhd_file")"
	echo "lib $library ${name%%.vhd} vhdl" >>"$PAO_FILE"
done
