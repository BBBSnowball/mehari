#!/bin/bash

set -e

##ZYNQ_BOARD_REVISION=d

[ -n "$MAKE_PARALLEL_PROCESSES" ] || MAKE_PARALLEL_PROCESSES=4

#SKIP_BUILDING_UBOOT=1
#SKIP_BUILDING_KERNEL=1
#SKIP_BUILDING_ROOTFS=1
#SKIP_BUILDING_RECONOS=1
#BOOT_ZYNQ=1

[ -n "$HOST_IP" ]  || HOST_IP=192.168.24.17
[ -n "$BOARD_IP" ] || BOARD_IP=192.168.24.23

[ -n "$DEMO" ]     || DEMO=sort_demo

#WARNING: FILES IN THIS DIRECTORY WILL BE OVERWRITTEN!
[ -n "$NFS_ROOT" ] || NFS_ROOT=/nfs/zynqn


################################################################################


cd "$(dirname "$0")"
ROOT="$(pwd)"

FILES="$ROOT/files"

# TODO: clone gits / update submodules if they are not available

# using commit 8d3efaf35eb9d13811456 for busybox

clean_repository() {
	( cd "$ROOT/$1" && git reset --hard && git clean -f -x -d )
}

if [ "$SKIP_BUILDING_UBOOT" -le 0 ] ; then
	clean_repository u-boot-xlnx
fi
if [ "$SKIP_BUILDING_KERNEL" -le 0 ] ; then
	clean_repository linux-xlnx
fi
if [ "$SKIP_BUILDING_ROOTFS" -le 0 ] ; then
	clean_repository busybox
fi
if [ "$SKIP_BUILDING_RECONOS" -le 0 ] ; then
	clean_repository reconos
fi


DEFAULT_CONFIG="./reconos/tools/environment/default.sh"
cat >"$DEFAULT_CONFIG" <<'EOF'
#!/bin/sh

xil_tools="/opt/Xilinx/14.6"
gnutools="$xil_tools/ISE_DS/EDK/gnu/arm/lin/bin/arm-xilinx-linux-gnueabi-"
reconos_arch="zynq"
reconos_os="linux"
reconos_mmu=true
EOF
echo "export KDIR=\"$ROOT/linux-xlnx\"" >>"$DEFAULT_CONFIG"
echo "export PATH=\"$ROOT/u-boot-xlnx/tools:\$PATH\"" >>"$DEFAULT_CONFIG"

. ./reconos/tools/environment/setup_reconos_toolchain.sh



make_parallel() {
	make -j"$MAKE_PARALLEL_PROCESSES" "$@"
}


if [ "$SKIP_BUILDING_UBOOT" -le 0 ] ; then


# TODO change IP in header file zynq_common.h

cd "$ROOT/u-boot-xlnx"
patch -p1 <"$FILES/u-boot-xlnx-zynq.patch"

cd "$ROOT/u-boot-xlnx"
make_parallel zynq_zed

fi		# not SKIP_BUILDING_UBOOT


if [ "$SKIP_BUILDING_KERNEL" -le 0 ] ; then
	cd "$ROOT/linux-xlnx"
	make xilinx_zynq_defconfig
	make_parallel uImage LOADADDR=0x00008000
fi		# not SKIP_BUILDING_KERNEL


if [ "$SKIP_BUILDING_ROOTFS" -le 0 ] ; then

cd "$ROOT/busybox"
cp "$FILES/busybox-config" ./.config
make_parallel
make install	# it is installed into CONFIG_PREFIX, which is "./_install"
cd _install
mkdir dev etc etc/init.d lib mnt opt proc root sys tmp var

cat >etc/init.d/rcS <<'EOF'
#!/bin/sh

echo "Starting rcS..."

echo "++ Mounting filesystem"
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp

ttydev=`cat /sys/class/tty/ttyPS0/dev`
ttymajor=${ttydev%%:*}
ttyminor=${ttydev##*:}
if [ -c /dev/ttyPS0 ]
then
	rm /dev/ttyPS0
fi

mknod /dev/ttyPS0 c $ttymajor $ttyminor

echo "rcS Complete"
EOF

chmod +x etc/init.d/rcS

cat >etc/inittab <<'EOF'
::sysinit:/etc/init.d/rcS

# /bin/sh
# 
# Start an askfirst shell on the serial ports

ttyPS0::respawn:-/bin/sh

# What to do when restarting the init process

::restart:/sbin/init

# What to do before rebooting

::shutdown:/bin/umount -a -r
EOF

cat >etc/fstab <<'EOF'
none        /proc       proc    defaults        0 0
none        /sys        sysfs   defaults        0 0
none        /tmp        tmpfs   defaults        0 0
EOF

fi		# not SKIP_BUILDING_ROOTFS



if [ "$SKIP_BUILDING_RECONOS" -le 0 ] ; then
	cd "$ROOT/reconos/linux/driver"
	make_parallel
	cd "$ROOT/reconos/linux/lib"
	make_parallel

	cd "$ROOT/reconos/demos/$DEMO/linux"
	make_parallel

	cd "$ROOT/reconos/demos/$DEMO/hw"
	#TODO put Xilinx tool version and board revision into setup_zynq
	#     sed -i 's/zedboard_minimal_[0-9].*/zedboard_minimal_14.6/' reconos/demos/sort_demo/hw/setup_zynq
	reconos_setup.sh setup_zynq

	cd "$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux"
	#TODO keep __xps folder in git and use `make -f system.make bits`
	echo "run bits ; exit" | xps -nw system

	cp "$FILES/device_tree.dts" .
	sed -i 's#\(nfsroot=[0-9.]\+\):[^,]*,tcp#nfsroot='"$HOST_IP"':'"$NFS_ROOT"',tcp#' device_tree.dts
	sed -i 's#\bip=[0-9.]\+:#ip='"$BOARD_IP"':#' device_tree.dts
	"$ROOT/linux-xlnx/scripts/dtc/dtc" -I dts -O dtb -o device_tree.dtb device_tree.dts
fi

# TODO the network-manager of ubuntu kills our network settings for eth0 -> disable it 

if [ "$BOOT_ZYNQ" -ge 1 ] ; then
	sudo mkdir -p "$NFS_ROOT"
	sudo cp -a "$ROOT/busybox/_install/." "$NFS_ROOT"
	
	# TODO diff will print error because we put test files into $NFS_ROOT/opt/reconos

	if ! sudo diff -r "$ROOT/busybox/_install" "$NFS_ROOT" ; then
		echo "WARNING: NFS root directory $NFS_ROOT contains additional files!" >&2
	fi

	sudo cp "$ROOT/reconos/linux/driver/mreconos.ko" "$NFS_ROOT/opt/reconos"
	sudo cp "$ROOT/reconos/linux/scripts/reconos_init.sh" "$NFS_ROOT/opt/reconos"
	sudo cp "$ROOT/reconos/demos/$DEMO/linux/$DEMO" "$NFS_ROOT/opt/reconos"

	X="$(tempfile)"
	cp /etc/exports "$X"
	grep -v "$NFS_ROOT" <"$X" | sudo tee /etc/exports >/dev/null
	rm "$X"
	echo "$NFS_ROOT $BOARD_IP(rw,no_root_squash,no_subtree_check)" | sudo tee -a /etc/exports >/dev/null
	sudo exportfs -rav

	#sudo ifconfig eth0:1 $HOST_IP up
	sudo ifconfig eth0 $HOST_IP up

	#TODO this will be in git...
	cp "$FILES/ps7_init.tcl" "$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/ps7_init.tcl"

	cd "$ROOT"
	zynq_boot_jtag.sh \
		"$ROOT/linux-xlnx/arch/arm/boot/uImage" \
		"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/device_tree.dtb" \
		"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/ps7_init.tcl" \
		"$ROOT/u-boot-xlnx/u-boot"

	echo "fpga -f \"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/implementation/system.bit\" ; exit" | xmd
fi
