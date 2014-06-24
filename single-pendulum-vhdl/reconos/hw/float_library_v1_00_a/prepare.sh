#!/bin/bash

set -e

cd "$(dirname "$0")"

library="$(basename "$(pwd)")"

# symlink generated ipcores into this library and add them to the *.pao file
DATA_DIR="data"
PAO_FILE="$DATA_DIR/float_library_v2_1_0.pao"
cat "$PAO_FILE.tpl" >"$PAO_FILE"
mkdir -p hdl/vhdl/
for vhd_file in ../../../ipcore_dir/*.vhd ; do
	ln -sf "../../$vhd_file" hdl/vhdl/

	name="$(basename "$vhd_file")"
	echo "lib $library ${name%%.vhd} vhdl" >>"$PAO_FILE"
done

BBD_FILE="$DATA_DIR/float_library_v2_1_0.bbd"
echo "FILES" >"$BBD_FILE"
first=yes
mkdir -p netlist
for ngc_file in ../../../ipcore_dir/*.ngc ; do
	ln -sf "../$ngc_file" netlist/

	name="$(basename "$ngc_file")"
	if [ -n "$first" ] ; then
		first=
	else
		echo -n ", " >>"$BBD_FILE"
	fi
	echo -n "$name" >>"$BBD_FILE"
done
echo "" >>"$BBD_FILE"
