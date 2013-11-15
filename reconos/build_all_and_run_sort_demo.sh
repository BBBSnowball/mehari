#!/bin/bash

set -e

##ZYNQ_BOARD_REVISION=d

[ -n "$MAKE_PARALLEL_PROCESSES" ] || MAKE_PARALLEL_PROCESSES=4

#SKIP_BUILDING_UBOOT=1
#SKIP_BUILDING_KERNEL=1
#SKIP_BUILDING_ROOTFS=1
#SKIP_BUILDING_RECONOS=1
#BOOT_ZYNQ=1
#DEBUG_SKIP_EXPENSIVE_OPERATIONS=1

[ -n "$HOST_IP" ]  || HOST_IP=192.168.24.17
[ -n "$BOARD_IP" ] || BOARD_IP=192.168.24.23

[ -n "$DEMO" ]     || DEMO=sort_demo

#WARNING: FILES IN THIS DIRECTORY WILL BE OVERWRITTEN!
[ -n "$NFS_ROOT" ] || NFS_ROOT=/nfs/zynqn


################################################################################


cd "$(dirname "$0")"
ROOT="$(pwd)"

FILES="$ROOT/files"

if ! [ -d "busybox" -a -d "linux-xlnx" -a -d "reconos" -a -d "u-boot-xlnx" ] ; then
	echo "ERROR: I need several gits with source code. At least one of them is missing."  >&2
	echo "  If you have received this as part of a git, init and update the submodules:"  >&2
	echo "    git submodule init reconos/{busybox,linux-xlnx,reconos,u-boot-xlnx}"        >&2
	echo "    git submodule update"                                                       >&2
	echo "  If you cannot use submodules, clone the gits individually:"                   >&2
	echo "    git clone git://git.busybox.net/busybox             busybox     -b 8d3efaf" >&2
	echo "    git clone https://github.com/Xilinx/linux-xlnx.git  linux-xlnx  -b efc2750" >&2
	echo "    git clone git@github.com:pc2/mehari-private.git     reconos     -b 9ca99b6" >&2
	echo "    git clone https://github.com/Xilinx/u-boot-xlnx.git u-boot-xlnx -b 4cb9c3d" >&2
	exit 1
fi


if (($BOOT_ZYNQ)) && ( ! ifconfig | grep -q "inet addr6\?:$HOST_IP[/ ]" \
                    || ! ping -c1 "$HOST_IP" >/dev/null ) ; then
	echo "ERROR: HOST_IP is $HOST_IP, but none of your interfaces has that IP."          >&2
	echo "  You have several options:"                                                   >&2
	echo "  - set $HOST_IP as static IP in your network settings"                        >&2
	echo "  - manually set an additional IP for one of your interfaces:"                 >&2
	echo "    sudo ifconfig eth0:1 $HOST_IP up"                                          >&2
	echo "    Please make sure that your network management software doesn't interfere." >&2
	echo "    In our tests, we had to stop the network-manager daemon. You also have to" >&2
	echo "    make sure that the interface isn't taken down, i.e. don't pull out the"    >&2
	echo "    ethernet cable and connect it via a switch. If you directly connect it to" >&2
	echo "    the board, the connection will be reset."                                  >&2
	echo "  - Change the IPs that we use:"                                               >&2
	echo "    HOST_IP=... BOARD_IP=... $0"                                               >&2
	echo "  - only build and don't run it:"                                              >&2
	echo "    BOOT_ZYNQ=0 $0"                                                            >&2
	exit 1
fi


clean_repository() {
	( cd "$ROOT/$1" && git reset --hard && git clean -f -x -d )
}

if ! (($SKIP_BUILDING_UBOOT)) ; then
	clean_repository u-boot-xlnx
fi
if ! (($SKIP_BUILDING_KERNEL)) ; then
	clean_repository linux-xlnx
fi
if ! (($SKIP_BUILDING_ROOTFS)) ; then
	clean_repository busybox
fi
if ! (($SKIP_BUILDING_RECONOS)) ; then
	clean_repository reconos
fi

if (($ONLY_CLEAN)) ; then
	exit
fi


RECONOS_CONFIG="./reconos/tools/environment/default.sh"
cp "$FILES/reconos-config.sh" "$RECONOS_CONFIG"
if [ -n "$XILINX" ] ; then
	sed -i 's#^xil_tools=.*$#xil_tools='"$XILINX/../.."'#' "$RECONOS_CONFIG"
elif [ -n "$XILINX_VERSION" ] ; then
	sed -i '/^xil_tools=/ s#/[0-9]\+\.[0-9]\b#/'"$XILINX_VERSION"'#' "$RECONOS_CONFIG"
fi
echo "export KDIR=\"$ROOT/linux-xlnx\"" >>"$RECONOS_CONFIG"
echo "export PATH=\"$ROOT/u-boot-xlnx/tools:\$PATH\"" >>"$RECONOS_CONFIG"

. ./reconos/tools/environment/setup_reconos_toolchain.sh

if [ -z "$XILINX_VERSION" ] ; then
	XILINX_VERSION=$(xps -help|sed -n 's/^Xilinx EDK \([0-9]\+\.[0-9]\+\)\b.*$/\1/p')
fi

long_operation() {
	if (($DEBUG_SKIP_EXPENSIVE_OPERATIONS)) ; then
		echo "Skipping: $*"
	else
		"$@"
	fi
}

make_parallel() {
	long_operation make -j"$MAKE_PARALLEL_PROCESSES" "$@"
}

sudo_save_into() {
	sudo tee "$1" >/dev/null
}

sudo_append_to() {
	sudo tee -a "$1" >/dev/null
}

sudo_modify_by() {
	FILE="$1"
	shift

	tmpfile="$(tempfile)"
	cp "$FILE" "$tmpfile"
	"$@" <"$tmpfile" | sudo_save_into /etc/exports
	rm "$tmpfile"
}

run_xmd() {
	echo "$* ; exit" | xmd
}

run_xps() {
	PROJECT="$1"
	shift
	echo "$* ; exit" | xps -nw "$PROJECT"
}

cd_verbose() {
	echo
	echo "In directory: $1"
	cd "$1"
}


if ! (($SKIP_BUILDING_UBOOT)) ; then
	# TODO change IP in header file zynq_common.h

	cd_verbose "$ROOT/u-boot-xlnx"
	patch -p1 <"$FILES/u-boot-xlnx-zynq.patch"

	sed -i 's/^#define CONFIG_IPADDR\b.*$/#define CONFIG_IPADDR   '"$BOARD_IP"'/'  include/configs/zynq_common.h
	sed -i 's/^#define CONFIG_SERVERIP\b.*$/#define CONFIG_SERVERIP '"$HOST_IP"'/' include/configs/zynq_common.h

	cd_verbose "$ROOT/u-boot-xlnx"
	make_parallel zynq_zed

fi	# not SKIP_BUILDING_UBOOT


if ! (($SKIP_BUILDING_KERNEL)) ; then
	cd_verbose "$ROOT/linux-xlnx"
	make xilinx_zynq_defconfig
	make_parallel uImage LOADADDR=0x00008000
fi	# not SKIP_BUILDING_KERNEL


if ! (($SKIP_BUILDING_ROOTFS)) ; then
	cd_verbose "$ROOT/busybox"
	cp "$FILES/busybox-config" ./.config
	if ! (($DEBUG_SKIP_EXPENSIVE_OPERATIONS)) ; then
		make_parallel
		long_operation make install	# it is installed into CONFIG_PREFIX, which is "./_install"
	else
		mkdir -p _install
	fi
	cd_verbose _install
	mkdir dev etc etc/init.d lib mnt opt proc root sys tmp var

	cp "$FILES/startup-script.sh" etc/init.d/rcS
	chmod +x                      etc/init.d/rcS
	cp "$FILES/inittab"           etc/inittab
	cp "$FILES/fstab"             etc/fstab
fi	# not SKIP_BUILDING_ROOTFS



if ! (($SKIP_BUILDING_RECONOS)) ; then
	cd_verbose "$ROOT/reconos/linux/driver"
	make_parallel
	cd_verbose "$ROOT/reconos/linux/lib"
	make_parallel

	cd_verbose "$ROOT/reconos/demos/$DEMO/linux"
	make_parallel

	cd_verbose "$ROOT/reconos/demos/$DEMO/hw"
	if [ -n "$XILINX_VERSION" ] ; then
		sed -i 's/zedboard_minimal_[0-9].*/zedboard_minimal_14.6/' setup_zynq
	fi
	reconos_setup.sh setup_zynq

	cd_verbose "$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux"
	#TODO keep __xps folder in git and use `make -f system.make bits`
	make_bitfile() {
		run_xps "system" "run bits"
	}
	long_operation make_bitfile

	cp "$FILES/device_tree.dts" .
	sed -i 's#\(nfsroot=[0-9.]\+\):[^,]*,tcp#nfsroot='"$HOST_IP"':'"$NFS_ROOT"',tcp#' device_tree.dts
	sed -i 's#\bip=[0-9.]\+:#ip='"$BOARD_IP"':#' device_tree.dts
	long_operation "$ROOT/linux-xlnx/scripts/dtc/dtc" -I dts -O dtb -o device_tree.dtb device_tree.dts
fi	# not SKIP_BUILDING_RECONOS

if (($BOOT_ZYNQ)) ; then
	sudo mkdir -p "$NFS_ROOT"
	sudo cp -a "$ROOT/busybox/_install/." "$NFS_ROOT"
	
	sudo rm -rf "$NFS_ROOT/opt/reconos"

	if ! sudo diff -r "$ROOT/busybox/_install" "$NFS_ROOT" ; then
		echo "WARNING: NFS root directory $NFS_ROOT contains additional files!" >&2
	fi

	sudo mkdir -p "$NFS_ROOT/opt/reconos"

	sudo cp "$ROOT/reconos/linux/driver/mreconos.ko"      "$NFS_ROOT/opt/reconos"
	sudo cp "$ROOT/reconos/linux/scripts/reconos_init.sh" "$NFS_ROOT/opt/reconos"
	sudo cp "$ROOT/reconos/demos/$DEMO/linux/$DEMO"       "$NFS_ROOT/opt/reconos"

	sudo_modify_by /etc/exports \
		grep -v "$NFS_ROOT"
	echo "$NFS_ROOT $BOARD_IP(rw,no_root_squash,no_subtree_check)" | sudo_append_to /etc/exports
	sudo exportfs -rav

	#TODO this should be in git...
	cp "$FILES/ps7_init.tcl" "$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/ps7_init.tcl"

	cd_verbose "$ROOT"
	zynq_boot_jtag.sh \
		"$ROOT/linux-xlnx/arch/arm/boot/uImage" \
		"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/device_tree.dtb" \
		"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/ps7_init.tcl" \
		"$ROOT/u-boot-xlnx/u-boot"

	run_xmd "fpga -f \"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/implementation/system.bit\""

	echo "The board should be booting, now."

	sleep 10

	if ! ping -c1 "$HOST_IP" >/dev/null ; then
		echo "ERROR: I cannot ping $HOST_IP, so for some reason your IP got reset."   >&2
		echo "  Please make sure that you use an ethernet switch (i.e. don't"         >&2
		echo "  directly connect your PC to the board) or make sure that the IP"      >&2
		echo "  settings survive a reset of the ethernet connection (i.e. set them"   >&2
		echo "  in NetworkManager or similar)."                                       >&2
		exit 1
	fi
	if ! ping -c1 "$BOARD_IP" >/dev/null ; then
		echo "WARNING: I cannot ping the board ($BOARD_IP). This is a bad sign."      >&2
	fi
fi	# BOOT_ZYNQ
