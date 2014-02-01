TODO: write a really good description... ;-)

Quickstart
==========

Get the code
------------

You need `git` to get the code. Please install it: `aptitude install git` or whatever is appropriate for your distribution. On Windows, you should install [`git`](http://git-scm.com/download/win) and [TortoiseGit](http://code.google.com/p/tortoisegit/downloads/list).

Some part of our examples are in a private git. If you do have access to this git, you can `git init` all submodules. If you don't have access to it, don't worry. You don't need it to work with Mehari.

````
# master repository
git clone git@github.com:BBBSnowball/mehari.git mehari

# clone submodules
# If you do have access to the private git, add 'private' at the end of this command.
cd mehari
git submodule init "reconos/linux-xlnx" "reconos/u-boot-xlnx" "reconos/reconos" "reconos/busybox" \
	"tools/python-ctemplate" "llvm/llvmpy/graph-llvm-ir"
git submodule update
cd ..
````


### Why do you use submodules?

We need more than one git for several reasons:

- If we keep the structure of the projects we use, we can easily merge updates and send patches.
- We are not allowed to publish part of the examples, so we cannot put them into the public repository.

If you don't want to (or cannot) clone all the submodules, you must be careful not to `git init` them. In that case, git will simply ignore them (tested with git 1.7.9.5 on Ubuntu 12.04). This means you shouldn't run `git init` without any arguments, as this would initialize all submodules. We are very sorry for the inconvenience that may cause. However, there is an important advantage of submodules: Git will track the versions of them, so you always get a consistent state. Without this information, building and testing the program would involve a lot of guesswork.

If you ever `git init` a submodule by accident, please try this procedure: `git stash` to save changes in `.gitmodules` (if there are any), completely remove the submodule (e.g. according to [this guide](http://davidwalsh.name/git-remove-submodule)) but DO NOT COMMIT, `git checkout HEAD .gitmodules` to remove any changes in the `.gitmodules` files, `git stash pop` to restore your changes that were saved by `git stash`.


### Why do you have a private git?

Most examples and the libraries they use are IP of iXtronics GmbH. Most of them are based on code that is used for real mechatronic systems or demonstration systems. We won't publish them without their permission. We have prepared some examples that don't expose an confidential information. Although they are not as extensive as the private examples, you can use them to test the software and do simple performance measurements.


### Why do I need git? Can't you provide tar balls like everyone else?

Mehari is geared to developers. If you use git, you can try different versions and record your own changes. If you prefer a different version control system, you can convert the git (e.g. [to mercurial](http://mercurial.selenic.com/wiki/ConvertExtension)).

If you only want the code once (i.e. you don't want updates or make any changes), you can download ZIP files on the repository web sites (e.g. for the [mehari repository](https://github.com/bbbsnowball/mehari)). We won't cover this - you will be on your own. I heartily recommend that you use `git`.

Install prerequisites and configure your system
-----------------------------------------------

### Linux system

We are using Ubuntu 12.04 (precise). You may want to use the same to avoid problems due to subtle differences between Linux distributions. However, our software should work with any Linux system. We are using a 64-bit system (amd64). If you are using 32-bit, you will have to tweak a few things to make it work. It may also work on Mac OS and Windows (msys or cygwin), but that won't work out-of-the-box. In any case, feel free to try it and please report your experience (or send patches).

### Install software

I hope your are using a Linux distribution that uses deb packages. If you do, simply run this:

```
# Tools used by the build scripts and/ or tests
# (sispmctl is optional)
sudo apt-get install curl git build-essential python-yaml python-unipath bc python2.7-dev libctemplate-dev gawk \
	build-essential sispmctl

# We need Java. You may want to use a different version.
sudo apt-get install openjdk-6-jre-headless

# Libraries needed by Xilinx tools
# (You most likely have most of them, unless you run a headless system.)
sudo apt-get install libsm6 libxi6 libxrender1 libxrandr2 libfontconfig1 libc6-i386

# Servers needed 
sudo apt-get install nfs-kernel-server tftpd tftp xinetd

# Some build scripts expect gmake instead of make, so we create a symlink
sudo ln -s make /usr/bin/gmake
```

If someone translates this to other Linux distributions, we will gladly add this information here.

You will also have to install Xilinx XPS. At the time of writing, we don't use any feature that requires more than a Webpack license, but that may change. We recommend version 14.6 or 14.7.

### Setup NFS

The NFS mounts are a bit special because we use the `all_squash` option to grant root permissions on the NFS share to the user who works with the board. Therefore, you should make sure that only trusted users can access the share. Of course, you can get rid of the `all_squash` mount and run the build scripts with `sudo`.

```
HOST_IP=...
BOARD_IP=...

cat >>/etc/exports <<EOF
/media/zynq-rootfs $HOST_IP(rw,async,all_squash,anongid=0,anonuid=0,secure,no_subtree_check) \
                   $BOARD_IP(rw,async,no_root_squash,secure,no_subtree_check)
EOF

cat >>/etc/fstab <<EOF
$HOST_IP:/media/zynq-rootfs /nfs/zynqn nfs auto,soft,vers=3,bg 0 0
EOF

mkdir /media/zynq-rootfs
exportfs -rav
mkdir -p /nfs/zynqn
mount /nfs/zynqn

# make sure only you can access the share (important!)
chown $YOUR_USER /nfs
chmod 700 /nfs
```

### Setup TFTP

Run these commands as user root (i.e. after `sudo -s`):

```
# copied from https://linuxlink.timesys.com/docs/linux_tftp
# but I think the file is from a Debian package
# (although it doesn't exist on my host)
cat >/etc/xinetd.d/tftpd <<'EOF'
# default: off
# description: The tftp server serves files using the Trivial File Transfer \
#    Protocol.  The tftp protocol is often used to boot diskless \
#    workstations, download configuration files to network-aware printers, \
#    and to start the installation process for some operating systems.
service tftp
{
    socket_type     = dgram
    protocol        = udp
    wait            = yes
    user            = nobody
    server          = /usr/sbin/in.tftpd
    server_args     = -s /tftp
    disable         = no 
}
EOF

service xinetd reload

# make sure you will be able to change the kernel and devicetree files
mkdir /tftp
touch            /tftp/uImage /tftp/devicetree.dtb
chown $YOUR_USER /tftp/uImage /tftp/devicetree.dtb
```

### Grant permissions to devices

You should be able to access these devices without using `sudo`:

    - serial2usb interface (`/dev/ttyACM*`)
    - JTAG interface of the ZedBoard (`/dev/ttyUSB*`)
    - Gembird SilverShield (optional)

On Ubuntu 12.04, you can use some udev rules to achieve this. I think this will be similar on your system. On Ubuntu, put these rules into `/etc/udev/rules.d/45-zedboard.rules`:

```
# serial-to-usb interface
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="04b4", ATTR{idProduct}=="0008", MODE="0666", GROUP="plugdev"

# JTAG interface (FTDI chipset with Digilent VID&PID)
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="0403", ATTR{idProduct}=="6014", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", ATTR{idProduct}=="6014", MODE="0666", GROUP="plugdev"

# Gembird SilverShield
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="04b4", ATTR{idProduct}=="fd11", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="04b4", ATTR{idProduct}=="fd13", MODE="0666", GROUP="plugdev"
```

You also have to make sure that your user is in the groups `plugdev` and `dialout`. If you change this, you have to logout and login again. You can check group membership with the `groups` command.

### Setup source repository

On our CI server, we use Artifactory to provide source artifacts of LLVM and some other tools. You can either use our Artifactory server or create a repository in a local directory. We don't have further information about this, yet.

### Create build configuration

Copy `gradle-example.properties` to `gradle-local.properties` and change the settings. In particular, there are these settings:

- `xilinx_settings_script`: path to `settings64.sh` in your Xilinx installation, e.g. `/opt/Xilinx/14.7/ISE_DS/settings64.sh`
- `xilinx_version`: version of the Xilinx tools, e.g. `14.7` (must match `xilinx_settings_script`)
- `parallel_compilation_processes`: how many parallel process to use for compilation (`-j` option for make, usually about the amount of processor cores you have)
- `sispm_outlet`: If you have a programmable power outlet that can be controlled by [`sispmctl`](http://sispmctl.sourceforge.net/) (e.g. Gembird SilverShield), set this to the outlet with the board. For example, set `sispm_outlet=1`, if the ZedBoard AC adapter is connected to the first outlet.
- `host_ip`: IP of your host system you want to use for communicating with the board.
- `board_ip`: IP of the board. It must be in the same "/24" subnet as `host_ip` (i.e. they must be able to communicate, if the board uses the netmask 255.255.255.0).
- `board_mac_address`: MAC address of the board. You can use any unused, valid MAC address. AFAIK, the ZedBoard doesn't have any MAC address of its own.
- `local_nfs_root`: NFS directory on the host. The build scripts must have root permissions in this location (i.e. they must be able to use `chown`).
- `nfs_mount`: NFS mount path for the board. Defaults to `host_ip:local_nfs_root`.

Build and test
--------------

We use [Gradle](http://www.gradle.org/) for our build scripts. You use the `gradlew` script to invoke it. It will automatically download all dependencies. You can use `gradlew` to execute tasks in our build scripts. Here are a few examples:

```
# do everything the continuous integration would do: clean, compile and test (and a few other things)
./gradlew --daemon allCI

# get at list of all tasks
./gradlew --daemon tasks --all
```

Java startup can be slow, so you may want to use the `--daemon` argument which keeps a JVM running in the background.

Do something useful
-------------------

We are still setting the foundations, so we don't have any finished tools, yet. If you are looking for something that works out-of-the-box and does something useful, you have to come back at a later time.
