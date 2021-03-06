#!/bin/bash

set -e

##ZYNQ_BOARD_REVISION=d

[ -n "$MAKE_PARALLEL_PROCESSES" ] || MAKE_PARALLEL_PROCESSES=4

[ -n "$HOST_IP" ]  || HOST_IP=192.168.24.17
[ -n "$BOARD_IP" ] || BOARD_IP=192.168.24.23

[ -n "$DEMO" ]     || DEMO=sort_demo

#WARNING: FILES IN THIS DIRECTORY WILL BE OVERWRITTEN!
[ -n "$NFS_ROOT" ] || NFS_ROOT=/nfs/zynqn


################################################################################

if (($SKIP_BUILDING)) ; then
	[ -z "$SKIP_BUILDING_UBOOT"    ] && SKIP_BUILDING_UBOOT=1
	[ -z "$SKIP_BUILDING_KERNEL"   ] && SKIP_BUILDING_KERNEL=1
	[ -z "$SKIP_BUILDING_BUSYBOX"  ] && SKIP_BUILDING_BUSYBOX=1
	[ -z "$SKIP_BUILDING_DROPBEAR" ] && SKIP_BUILDING_DROPBEAR=1
	[ -z "$SKIP_BUILDING_RECONOS"  ] && SKIP_BUILDING_RECONOS=1
	[ -z "$SKIP_BUILDING_ROOTFS"   ] && SKIP_BUILDING_ROOTFS=1
fi

if ! (($SKIP_BUILDING_BUSYBOX)) || ! (($SKIP_BUILDING_DROPBEAR)) || ! (($SKIP_BUILDING_RECONOS)) ; then
	SKIP_BUILDING_ROOTFS=0
fi

if (($BOOT_ZYNQ)) ; then
	[ -z "$UPDATE_NFSROOT" ] && UPDATE_NFSROOT=1
fi


cd "$(dirname "$0")"
ROOT="$(pwd)"

FILES="$ROOT/files"

ROOTFS="$ROOT/_install"

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
	( cd "$ROOT/$1" && git reset --hard && git clean -f -x -d ) || echo "Cleaning returned error status."
}

if ! (($SKIP_BUILDING_UBOOT)) ; then
	clean_repository u-boot-xlnx
fi
if ! (($SKIP_BUILDING_KERNEL)) ; then
	clean_repository linux-xlnx
fi
if ! (($SKIP_BUILDING_BUSYBOX)) ; then
	clean_repository busybox
fi
if ! (($SKIP_BUILDING_DROPBEAR)) ; then
	( cd dropbear ; [ -e "Makefile" ] && make distclean || true )
fi
if ! (($SKIP_BUILDING_RECONOS)) ; then
	clean_repository reconos
fi
if ! (($SKIP_BUILDING_ROOTFS)) ; then
	rm -rf "$ROOTFS"
fi

if (($ONLY_CLEAN)) ; then
	echo "Exit after cleaning."
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

if ! (($SKIP_BUILDING_ROOTFS)) ; then
	# make sure we have the most important libc files
	if [ -z "$libc_dir" -o ! -d "$libc_dir" ] ; then
		echo "ERROR: \$libc_dir must be set to the install directory of libc, but it doesn't exist" >&2
		exit 1
	fi
	for file in "usr/bin/getent" "lib/libutil.so.*" "lib/libc.so.*" "lib/ld-linux.so.*" \
				"lib/libgcc_s.so.*" "lib/libcrypt.so.*" "usr/lib/libnss_files.so" ; do
		if [ ! -e "$libc_dir"/$file ] ; then
			echo "ERROR: \$libc_dir must be set to the install directory of libc"              >&2
			echo "       At least the file '$file' is missing."                                >&2
			echo "       Please make sure that you install a complete libc (e.g. eglibc)"      >&2
			echo "       at this location. You have to provide more than the files I ask for!" >&2
			echo "       \$libc_dir is '$libc_dir'"                                            >&2
			exit 1
		fi
	done
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
	"$@" <"$tmpfile" | sudo_save_into "$FILE"
	rm "$tmpfile"
}

run_xmd() {
	echo "$* ; exit" | xmd
}

run_xps() {
	PROJECT="$1"
	shift
	echo "$* ; exit" '|' xps -nw "$PROJECT"
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
	make_parallel modules
fi	# not SKIP_BUILDING_KERNEL


if ! (($SKIP_BUILDING_BUSYBOX)) ; then
	cd_verbose "$ROOT/busybox"
	cp "$FILES/busybox-config" ./.config
	sed -i "s#^CONFIG_PREFIX=.*#CONFIG_PREFIX=\"$ROOTFS\"#" ./.config
	make_parallel
fi	# not SKIP_BUILDING_BUSYBOX

if ! (($SKIP_BUILDING_DROPBEAR)) ; then
	cd_verbose "$ROOT/dropbear"
	#TODO --disable-syslog ?
	HOST="$(basename "$CROSS_COMPILE" | sed 's/-$//')"
	./configure --prefix="$ROOTFS" --host="$HOST" --enable-bundled-libtom --disable-utmp --disable-zlib
	
	#NOTE We could modify options.h, now.

	TOOLCHAIN_BIN_DIR="$(dirname "$CROSS_COMPILE")"

	PATH="$TOOLCHAIN_BIN_DIR:$PATH" make_parallel
fi	# not SKIP_BUILDING_DROPBEAR

if ! (($SKIP_BUILDING_RECONOS)) ; then
	cd_verbose "$ROOT/reconos/linux/driver"
	make_parallel
	cd_verbose "$ROOT/reconos/linux/lib"
	make_parallel

	cd_verbose "$ROOT/reconos/demos/$DEMO/linux"
	make_parallel

	cd_verbose "$ROOT/reconos/demos/$DEMO/hw"
	if [ -n "$XILINX_VERSION" ] ; then
		#TODO get design for 14.7 from Christoph
		#sed -i 's/zedboard_minimal_[0-9].*/zedboard_minimal_'"$XILINX_VERSION"'/' setup_zynq
		echo "Not setting version"	#DEBUG
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

if ! (($SKIP_BUILDING_ROOTFS)) ; then
	mkdir -p "$ROOTFS"

	cd_verbose "$ROOT/busybox"
	make_parallel CONFIG_PREFIX="$ROOTFS" install
	if [ -e "$ROOT/busybox/busybox.links" ] ; then
		# hide this file, so it doesn't make the git dirty
		# (unfortunately, it is not ignored in .gitignore)
		mv "$ROOT/busybox/busybox.links" "$ROOT/busybox/.busybox.links"
	fi

	cd_verbose "$ROOTFS"
	mkdir dev dev/pts etc etc/{init.d,rc.d,default} lib mnt opt proc root sys tmp var var/{run,log}

	cp "$FILES/startup-script.sh" etc/rcS
	chmod +x                      etc/rcS
	cp "$FILES/inittab"           etc/inittab
	cp "$FILES/fstab"             etc/fstab

	cp "$FILES/init.d"/* "$ROOTFS/etc/init.d"
	chmod +x "$ROOTFS/etc/init.d"/*

	function get_init_info() {
		file="$1"
		key="$2"
		sed -n '/^#\+\s*BEGIN INIT INFO\s*$/,/^#\+\s*END INIT INFO\s*$/ s/^#\s*'"$key"':\s*\(.*\)$/\1/pi' "$file"
	}

	function get_order() {
		file="$1"
		start_or_stop="$2"

		ORDER="$(get_init_info "$file" "${start_or_stop}-order")"
		if [ -n "$ORDER" ] ; then
			echo "$ORDER"
		else
			ORDER="$(get_init_info "$file" "order")"

			if [ "$start_or_stop" == "start" ] ; then
				echo "$ORDER"
			else
				if [ -n "$ORDER" ] ; then
					printf "%02u" $[100-$ORDER]
				fi
			fi
		fi
	}

	cd_verbose "$ROOTFS/etc/init.d"
	rm -f "$ROOTFS/etc/rc.d/*"
	for rcscript in * ; do
		START_ORDER="$(get_order "$rcscript" start)"
		STOP_ORDER="$(get_order "$rcscript" stop)"

		if [ -n "$START_ORDER" ] ; then
			ln -s "../init.d/$rcscript" "../rc.d/S${START_ORDER}$rcscript"
		fi

		if [ -n "$STOP_ORDER" ] ; then
			ln -s "../init.d/$rcscript" "../rc.d/K${STOP_ORDER}$rcscript"
		fi
	done

	echo "NOTIFY_HOST=\"$HOST_IP\"" >"$ROOTFS/etc/default/notify-host"


	echo "root:x:0:0:root:/root:/bin/ash" >"$ROOTFS/etc/passwd"
	echo "root:x:0:"                      >"$ROOTFS/etc/group"

	# see http://cross-lfs.org/view/clfs-sysroot/arm/cross-tools/eglibc.html
	sed 's/^\s*//' > "$ROOTFS/etc/nsswitch.conf" << "EOF"
		passwd: files
		group: files
		shadow: files

		hosts: files dns
		networks: files

		protocols: files
		services: files
		ethers: files
		rpc: files
EOF
	
	cp /etc/localtime "$ROOTFS/etc/localtime"

	# eglibc doc (see above) suggest that we add `/usr/local/lib` and `/opt/lib`,
	# but we don't use them
	touch "$ROOTFS/etc/ld.so.conf"

	# copy libc files
	for dir in "usr/bin" "lib" "usr/lib" "usr/share" "sbin" "usr/sbin" ; do
		cp -a "$libc_dir/$dir/." "$ROOTFS/$dir"
	done



	cd_verbose "$ROOT/dropbear"
	PATH="$TOOLCHAIN_BIN_DIR:$PATH" make_parallel prefix="$ROOTFS" install

	mkdir -p "$ROOTFS/root/.ssh"
	if [ -e ~/.ssh/id_rsa.pub -a ! -e "$ROOTFS/root/.ssh/authorized_keys" ] ; then
		cat ~/.ssh/id_rsa.pub >"$ROOTFS/root/.ssh/authorized_keys"
		chmod 600 "$ROOTFS/root/.ssh/authorized_keys"
	fi

	if [ ! -e "$ROOTFS/etc/shells" ] || ! grep -q "^/bin/ash$" "$ROOTFS/etc/shells" ; then
		echo "/bin/ash" >>"$ROOTFS/etc/shells"
	fi


	cd_verbose "$ROOT/linux-xlnx"
	make_parallel INSTALL_MOD_PATH="$ROOTFS" modules_install


	mkdir -p "$ROOTFS/opt/reconos"

	KERNEL_RELEASE="$(cat "$ROOT/linux-xlnx/include/config/kernel.release")"
	RECONOS_MODULE="kernel/drivers/mreconos.ko"
	MODULE_DIR="$ROOTFS/lib/modules/$KERNEL_RELEASE"
	cp "$ROOT/reconos/linux/driver/mreconos.ko" "$MODULE_DIR/$RECONOS_MODULE"
	echo "$RECONOS_MODULE: " >>"$MODULE_DIR/modules.dep"
fi	# not SKIP_BUILDING_ROOTFS

if (($UPDATE_NFSROOT)) ; then
	sudo mkdir -p "$NFS_ROOT"
	sudo cp -a "$ROOTFS/." "$NFS_ROOT"
	
	if ! sudo diff -r "$ROOTFS" "$NFS_ROOT" -x .ash_history -x dropbear -x motd -x log -x run ; then
		echo "WARNING: NFS root directory $NFS_ROOT contains additional files!" >&2
	fi

	if [ -e "$NFS_ROOT/root/.ssh/authorized_keys" ] ; then
		sudo chown root:root "$NFS_ROOT/root" "$NFS_ROOT/root/.ssh" "$NFS_ROOT/root/.ssh/authorized_keys"
		sudo chmod 700 "$NFS_ROOT/root" "$NFS_ROOT/root/.ssh"
		sudo chmod 600 "$NFS_ROOT/root/.ssh/authorized_keys"
	fi
fi	# UPDATE_NFSROOT

if (($BOOT_ZYNQ)) ; then
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
	#NOTE If this fails because it doesn't find the cable, install the cable drivers:
	#     cd $XILINX/bin/lin64/digilent ; ./install_digilent.sh

	# The Digilent driver installer makes ~/.cse owned by root (or probably we shouldn't
	# have run it with sudo). xmd tries to put a temporary file into it and fails. Therefore,
	# we fix this.
	if [ -e ~/.cse ] && ls -l ~/.cse | awk '{print $3 $4}' | grep -q root ; then
		sudo chown -R "$USER" ~/.cse
	fi

	run_xmd "fpga -f \"$ROOT/reconos/demos/$DEMO/hw/edk_zynq_linux/implementation/system.bit\""

	echo "The board should be booting, now."
	echo "If you abort u-boot for some reason, enter 'jtagboot' or 'bootm 0x3000000 - 0x2A00000'."


	# wait for the board to contact us
	if ! timeout 30 nc -l "$HOST_IP" 12342 ; then
		echo "ERROR: The board didn't send the 'done' message within 30 seconds."         >&2

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

		exit 1
	fi
fi	# BOOT_ZYNQ

if (($PREPARE_SSH)) || (($SSH_SHELL)) || (($RUN_DEMO)) ; then
	get_board_pubkey() {
		keytype="$1"
		grep "^ssh-" "$NFS_ROOT/etc/dropbear/dropbear_${keytype}_host_key.info" | awk '{print $1 " " $2}'
	}
	BOARD_PUBKEY="$(get_board_pubkey rsa)"
	if [ -n "$BOARD_PUBKEY" ] && ! ssh-keygen -F "$BOARD_IP" | grep -q "$BOARD_PUBKEY" ; then
		# board key is not in known_hosts, so we remove any old ones and add the new one
		ssh-keygen -R "$BOARD_IP"
		echo "$BOARD_IP $BOARD_PUBKEY" >>~/.ssh/known_hosts
		# re-hash the file
		ssh-keygen -H
	fi
fi	# PREPARE_SSH

ssh_zynq() {
	ssh -o PubkeyAuthentication=yes root@"$BOARD_IP" "$@"
}

if (($SSH_SHELL)) ; then
	ssh_zynq
fi	# SSH_SHELL

if (($RUN_DEMO)) ; then
	[ -z "$DEMO_ARGS" ] && DEMO_ARGS="2 2 50"
	ssh_zynq "cat >/tmp/$DEMO" <"$ROOT/reconos/demos/$DEMO/linux/$DEMO"
	ssh_zynq "chmod +x /tmp/$DEMO && /tmp/$DEMO" $DEMO_ARGS
fi	# RUN_DEMO
