#!/usr/bin/env python

if __name__ == '__main__':
    __requires__ = 'Paver==1.2.2'
    import sys
    from unipath import Path

    # make sure we use our extended paver
    ROOT = Path(__file__).absolute().parent
    sys.path.insert(1, Path(ROOT.parent, "tools", "paver"))
    
    import paver.tasks

    sys.exit(paver.tasks.main())

from paver.easy import task, needs, might_call, register_cmdoptsgroup, cmdoptsgroup
import paver
#TODO use path.py that is bundled with paver
from unipath import Path
import sys, os, re, subprocess, glob, tempfile, logging


ROOT   = Path(__file__).absolute().parent
FILES  = Path(ROOT, "files")
ROOTFS = Path(ROOT, "_install")

sys.path.insert(1, Path(ROOT.parent, "tools", "python-helpers"))
from mehari.build_utils import *

logger = logging.getLogger(__name__)
logging.basicConfig()
if paver.tasks.environment.verbose:
    loglevel = logging.DEBUG
else:
    loglevel = logging.INFO
logger.setLevel(loglevel)
logging.getLogger().setLevel(loglevel)

# test only, don't do anything
NO_OP = from_env("NOOP", from_env("NO_OP", False))

if NO_OP:
    def sh(cmd):
        logger.info("sh-noop: %r" % (cmd,))
    def sh_test(cmd):
        logger.info("sh-noop?: %r" % (cmd,))
        logger.debug("  -> 0 (always 0 in no-op mode)")
        return 0
    import mehari.build_utils
    mehari.build_utils.sh = sh
    mehari.build_utils.sh_test = sh_test


reconos_build_options = register_cmdoptsgroup("reconos_build",
    ("parallel-processes=", "j", "Parallel processes started when building a subtask with GNU make"),
    ("host-ip=", "H", "IP address (of your machine) that should be used to communicate with the Zynq board"),
    ("board-ip=", "b", "IP address that will be used by the Zynq board"),
    ("demo=", "d", "Demo application to run"),
    ("nfs-root=", "r", "Location of the NFS root folder on your machine"),
    ("nfs-mount=", "", "IP of the NFS server (default: host-ip:nfs-root, i.e. 192.168.24.17:/nfs/zynqn). "
        + "If you change this default, make sure that nfs-root on this host referes to the same folder. "
        + "Furthermore, the script won't try to publish the NFS share (as it normally would), so make sure "
        + "that this share is configured and working."),
    ("nfs-no-sudo", "", "Do not use sudo to access nfs-root. The script assumes that it can edit all files "
        + "in nfs-root and files will automatically be owned by root. Here is one way to do it: nfs-root "
        + "is an nfs mount with options 'all_squash,anongid=0,anonuid=0' (in /etc/exports on the server, not "
        + "mount options!). You should point the board to the same NFS server using the nfs-mount option. "
        + "However, it should not be squashed at all (i.e. don't use all_squash, but do use no_root_squash)."),
    {"parallel_processes": "4", "host_ip": "192.168.24.17", "board_ip": "192.168.24.23",
     "demo": "sort_demo", "nfs_root": "/nfs/zynqn", "nfs-mount": "", "nfs-no-sudo": False})

# To be honest, all those options are quite confusing, so I will try to explain them. There are two
# scenarios to consider:
# 1. Your system can run an NFS server and you can use sudo to copy the files to the NFS share
#    and chown them to root.
# 2. You don't have an NFS server on this host or you don't have root permissions.
#
# The first case is quite simple: You set host-ip and board-ip to appropriate values and run this script.
# It will export the NFS share and use sudo whenever appropriate. You may have to enter your password for
# sudo. If you want the script to run unattended, change /etc/sudoers to not require a password.
#
# In addition, you may have to change your IP settings. Simply use the host-ip and board-ip parameters
# to tell the script which addresses it should use. Only IPv4 is supported. Both IPs must be in the same
# subnet assuming a netmask of 255.255.255.0 (.../24).
#
#
# The second case needs a bit more work. You may wonder why somebody would want to jump to so many hoops, if
# they could simply use the other solution. My continuous integration runs in an LXC container and my kernel
# is too old to support NFS inside containers. Furthermore, I don't want the tests to run with root priviledges.
#
# You have to prepare the NFS server:
# (I assume the NFS server has the IP 192.168.24.20 and host-ip and board-ip are the defaults.)
#   in /etc/exports on the NFS server:
#   /media/zynq-rootfs 192.168.24.17(rw,async,all_squash,anongid=0,anonuid=0,secure,no_subtree_check) \
#                      192.168.24.23(rw,async,no_root_squash,secure,no_subtree_check)
# Make sure that an appropriate file system lives at /media/zynq-rootfs. After changing /etc/exports
# you have to run something like 'exportfs -rav'.
#
# You have to mount the NFS share on the build host, i.e.
# ssh root@192.168.24.17 mount -t nfs 192.168.24.20:/media/zynq-rootfs /nfs/zynqn -o soft,vers=3
# You should add the mount to /etc/fstab, so it will be remounted after a reboot.
#
# The mounted NFS share provides full access for every user, so you should make sure that only trusted users
# can access it, e.g. chown jenkins /nfs ; chmod 700 /nfs
#
# Use these options: --nfs-mount=192.168.24.20:/media/zynq-rootfs --nfs-no-sudo


@task
def check_submodules():
    for source_dir in ["busybox", "linux-xlnx", "reconos", "u-boot-xlnx"]:
        if not Path(source_dir).isdir():
            logger.error(heredoc("""
                I need several gits with source code. At least '%s' is missing.
                  If you have received this as part of a git, init and update the submodules
                    git submodule init reconos/{busybox,linux-xlnx,reconos,u-boot-xlnx}
                    git submodule update
                  If you cannot use submodules, clone the gits individually:
                    git clone git://git.busybox.net/busybox             busybox     -b 8d3efaf
                    git clone https://github.com/Xilinx/linux-xlnx.git  linux-xlnx  -b efc2750
                    git clone git@github.com:pc2/mehari-private.git     reconos     -b 9ca99b6
                    git clone https://github.com/Xilinx/u-boot-xlnx.git u-boot-xlnx -b 4cb9c3d
                """ % source_dir))
            sys.exit(1)

def host_has_ip(ip):
    return sh_test('ifconfig | grep -q "inet addr6\?:%s[/ ]"' % ip) == 0

def can_ping(host):
    return sh_test('ping -c1 "%s" >/dev/null' % host) == 0

@task
@cmdoptsgroup("reconos_build")
def check_host_ip():
    """make sure this host responds to the host IP"""
    if not host_has_ip(reconos_build_options.host_ip) or not can_ping(reconos_build_options.host_ip):
        logger.error(heredoc("""
            ERROR: Host IP is %s, but none of your interfaces has that IP.
              You have several options:
              - set %s as static IP in your network settings
              - manually set an additional IP for one of your interfaces:
                sudo ifconfig eth0:1 %s up
                Please make sure that your network management software doesn't interfere.
                In our tests, we had to stop the network-manager daemon. You also have to
                make sure that the interface isn't taken down, i.e. don't pull out the
                ethernet cable and connect it via a switch. If you directly connect it to
                the board, the connection will be reset.
              - Change the IPs that we use:
                ./pavement.py boot_zynq --host-ip=... --board-ip=...
              - only build and don't run it
            """ % (reconos_build_options.host_ip, reconos_build_options.host_ip, reconos_build_options.host_ip)))
        sys.exit(1)

@task
@cmdoptsgroup("reconos_build")
@needs("check_submodules")
def reconos_config():
    RECONOS_CONFIG = Path(ROOT, "reconos", "tools", "environment", "default.sh")

    if "xil_tools" in os.environ:
        xil_tools = Path(os.environ["xil_tools"])
    elif "XILINX" in os.environ:
        xil_tools = Path(os.environ["XILINX"]).parent.parent
    elif "XILINX_VERSION" in os.environ:
        xil_tools = Path("/opt/Xilinx", os.environ["XILINX_VERSION"])
    else:
        xil_tools = Path("/opt/Xilinx/14.6")

    if not os.path.exists(xil_tools):
        logger.error(heredoc("""
            ERROR: Couldn't find the directory with Xilinx tools. Please install Xilinx ISE_DS
              (Webpack will do) and tell me about the location. You have several options:
            - Set the environment variable XILINX=/path/to/ISE_DS/ISE
            - Set the environment variable XILINX_VERSION, if the tools live in /opt/Xilinx/$XILINX_VERSION
            - Set the environment variable xil_tools to the directory that contains the ISE_DS directory
              (This may not work with other scripts, so you may want to choose one of the other options.)
            """))
        sys.exit(1)

    gnutools = Path(from_env("gnutools",
        Path(xil_tools, "ISE_DS", "EDK", "gnu", "arm", "lin", "bin", "arm-xilinx-linux-gnueabi-")))
    libc_dir = Path(from_env("libc_dir",
        Path(gnutools.parent.parent, "arm-xilinx-linux-gnueabi", "libc")))

    do_check_libc(libc_dir)

    Path(RECONOS_CONFIG).write_file(heredoc("""
        xil_tools="%s"
        gnutools="%s"
        libc_dir="%s"
        reconos_arch="zynq"
        reconos_os="linux"
        reconos_mmu=true

        export KDIR="%s/linux-xlnx"
        export PATH="%s/u-boot-xlnx/tools:$PATH"
        """ % (xil_tools, gnutools, libc_dir, ROOT, ROOT)))

    ROOT.chdir()
    source_shell_file("./reconos/tools/environment/setup_reconos_toolchain.sh")

    return {
        'xil_tools':    xil_tools,
        'gnutools':     gnutools,
        'libc_dir':     libc_dir,
        'reconos_arch': "zynq",
        'reconos_os':   "linux",
        'reconos_mmu':  True,
        'kernel_dir':   Path(ROOT, "linux-xlnx")
    }

def get_libc_dir_after_reconos_config():
    #TODO we should use the return value of the reconos task, but unfortunately,
    #     Task.__call__ always returns None
    gnutools = Path(os.environ["CROSS_COMPILE"])
    return Path(gnutools.parent.parent, "arm-xilinx-linux-gnueabi", "libc")

XILINX_VERSION = None
def xilinx_version():
    global XILINX_VERSION
    if XILINX_VERSION:
        return XILINX_VERSION

    if "XILINX_VERSION" in os.environ:
        XILINX_VERSION = os.environ["XILINX_VERSION"]
    else:
        XILINX_VERSION = backticks(
            r"xps -help|sed -n 's/^Xilinx EDK \([0-9]\+\.[0-9]\+\)\b.*$/\1/p'")

    return XILINX_VERSION

def do_check_libc(libc_dir):
    # make sure we have the most important libc files
    if not libc_dir.isdir():
        logger.error("$libc_dir must be set to the install directory of libc, but it doesn't exist")
        sys.exit(1)
    for file in ["usr/bin/getent", "lib/libutil.so.*", "lib/libc.so.*", "lib/ld-linux.so.*",
                 "lib/libgcc_s.so.*", "lib/libcrypt.so.*", "usr/lib/libnss_files.so"]:
        p = Path(libc_dir, *file.split("/"))
        if len(glob.glob(p)) <= 0:
            logger.error(heredoc("""
                ERROR: \$libc_dir must be set to the install directory of libc
                       At least the file '%s' is missing.
                       Please make sure that you install a complete libc (e.g. eglibc)
                       at this location. You have to provide more than the files I ask for!
                       \$libc_dir is '%s'
                """ % (file, libc_dir)))
            sys.exit(1)

@task
@might_call("reconos_config")
def check_libc():
    libc_dir = reconos_config()["libc_dir"]

    do_check_libc(libc_dir)

@task
@might_call("reconos_config")
def check_digilent_driver():
    if sh_test("find ~/.cse -name libCseDigilent.so >/dev/null") != 0:
        logger.warning(heredoc("""
            WARNING: I cannot find the Digilent JTAG driver in ~/.cse. It may live in another location.
                     If you are sure that it works, you can ignore this warning. You can run these commands to
                     install it (adapt the path as appropriate):
                       cd /opt/Xilinx/14.7/ISE_DS/ISE/bin/lin64/digilent
                       sudo ./install_digilent.sh
                       sudo chown -R "$USER" ~/.cse
                       sudo cp -r ~/.cse ~some_user/
                       sudo chown -R some_user ~some_user/.cse
            """))

@task
@needs("check_submodules", "check_libc", "check_host_ip", "check_digilent_driver")
def check():
    pass


def clean_repository(dir):
    if not Path(dir, ".git").exists():
        raise IOError("'%s' is not the root of a git repository" % dir)
    sh('cd "%s" && git reset --hard && git clean -f -x -d' % dir)

@task
def clean_uboot():
    clean_repository("u-boot-xlnx")
@task
def clean_kernel():
    clean_repository("linux-xlnx")
@task
def clean_busybox():
    clean_repository("busybox")
@task
def clean_dropbear():
    sh("cd dropbear ; [ -e \"Makefile\" ] && make distclean || true")
@task
def clean_reconos():
    clean_repository("reconos")
@task
def clean_rootfs():
    Path(ROOTFS).rmtree()

@task
@needs("clean_uboot", "clean_kernel", "clean_busybox", "clean_dropbear",
    "clean_reconos", "clean_rootfs")
def clean():
    pass

@task
@needs("build_uboot", "build_kernel", "build_busybox", "build_dropbear",
    "build_reconos", "build_rootfs")
def build():
    pass

def long_operation(cmd):
    if int(from_env("DEBUG_SKIP_EXPENSIVE_OPERATIONS", "0")) > 0:
        logger.info("Skipping: " + str(cmd))
    else:
        if callable(cmd):
            cmd()
        else:
            sh(cmd)

def make(*targets):
    long_operation("make " + escape_for_shell(targets))

def make_parallel(*targets):
    long_operation(("make -j%s " % reconos_build_options.parallel_processes) + escape_for_shell(targets))


def sudo_cmd():
    if reconos_build_options.nfs_no_sudo:
        return ""
    else:
        return "sudo"

def sudo_modify_by(file, *command):
    x, tmpfile = tempfile.mkstemp()
    Path(file).copy(tmpfile)
    sh(ShellEscaped("%s <%s | %s tee %s >/dev/null")
        % (command, tmpfile, sudo_cmd(), file))
    Path(tmpfile).remove()

def run_xmd(commands):
    commands = escape_for_shell(str(commands) + " ; exit")
    sh("echo %s | xmd" % commands)

def run_xps(project, commands):
    cmd = ShellEscaped("echo %s | xps -nw %s") % (str(commands) + " ; exit", project)
    # try 3 times
    for i in range(3):
        res = sh_test(cmd)
        if res == 0:
            return
        elif res == 35584:
            logger.warn("xps failed. This may be due to a segfault, so we try again.")
        else:
            break
    # Either it didn't fail with a segfault or it failed too often
    raise IOError("System command returned non-zero status (return value is %d): %r"
        % (res, cmd))

def cd_verbose(*dir):
    dir = Path(*dir)
    logger.info("In directory: " + str(dir))
    dir.chdir()

def sed_i(expr, file):
    if isinstance(expr, (tuple, list)):
        exprs = map(escape_for_shell, expr)
        expr = "-e " + " -e ".join(exprs)
    else:
        expr = escape_for_shell(expr)
    file = escape_for_shell(file)
    sh('sed -i %s %s' % (expr, file))

@task
@cmdoptsgroup("reconos_build")
@needs("reconos_config")
def build_uboot():
    if "CROSS_COMPILE" not in os.environ:
        raise Exception("reconos_config didn't set CROSS_COMPILE")

    cd_verbose(ROOT, "u-boot-xlnx")
    sh(r'patch -N -p1 <"%s/u-boot-xlnx-zynq.patch"' % (FILES,))

    sed_i([r's/^#define CONFIG_IPADDR\b.*$/#define CONFIG_IPADDR   %s/'   % reconos_build_options.board_ip,
           r's/^#define CONFIG_SERVERIP\b.*$/#define CONFIG_SERVERIP %s/' % reconos_build_options.host_ip],
           "include/configs/zynq_common.h")

    cd_verbose(ROOT, "u-boot-xlnx")
    make_parallel("zynq_zed")


@task
@needs("reconos_config")
def build_kernel():
    cd_verbose(ROOT, "linux-xlnx")
    make("xilinx_zynq_defconfig")
    make_parallel("uImage", "LOADADDR=0x00008000")
    make_parallel("modules")


@task
@needs("reconos_config")
def build_busybox():
    cd_verbose(ROOT, "busybox")
    Path(FILES, "busybox-config").copy(".config")
    sed_i(r's#^CONFIG_PREFIX=.*#CONFIG_PREFIX=\"%s\"#' % ROOTFS.replace("#", "\\#"),
        ".config")
    make_parallel()


@task
@needs("reconos_config")
def build_dropbear():
    cd_verbose(ROOT, "dropbear")
    CROSS_COMPILE = os.environ["CROSS_COMPILE"]
    HOST = re.sub("-$", "", Path(CROSS_COMPILE).name)
    sh("./configure --prefix=%s --host=%s --enable-bundled-libtom --disable-utmp --disable-zlib"
            % (escape_for_shell(ROOTFS), escape_for_shell(HOST)))
    
    #NOTE We could modify options.h, now.

    TOOLCHAIN_BIN_DIR = Path(CROSS_COMPILE).parent

    #TODO does this work?
    path_with_toolchain = TOOLCHAIN_BIN_DIR + ":" + os.environ["PATH"]
    make_parallel(ShellEscaped("PATH=%s" % escape_for_shell(path_with_toolchain)))

@task
@cmdoptsgroup("reconos_build")
@needs("reconos_config")
def build_reconos():
    cd_verbose(ROOT, "reconos", "linux", "driver")
    make_parallel()
    cd_verbose(ROOT, "reconos", "linux", "lib")
    make_parallel()

    cd_verbose(ROOT, "reconos", "demos", reconos_build_options.demo, "linux")
    make_parallel()

    cd_verbose(ROOT, "reconos", "demos", reconos_build_options.demo, "hw")

    xil_version = xilinx_version()
    #TODO get design for 14.7 from Christoph
    #sed -i 's/zedboard_minimal_[0-9].*/zedboard_minimal_'"$XILINX_VERSION"'/' setup_zynq
    logger.info("Not setting version")  #DEBUG

    sh("bash reconos_setup.sh setup_zynq")

    cd_verbose(ROOT, "reconos", "demos", reconos_build_options.demo, "hw", "edk_zynq_linux")
    #TODO keep __xps folder in git and use `make -f system.make bits`
    #TODO This sometimes fails with a segfault (prints "Segmentation fault" and
    #     exits with status 35584). We should run it again in that case.
    def make_bitstream():
        run_xps("system", "run bits")
    long_operation(make_bitstream)

    Path(FILES, "device_tree.dts").copy(Path(".", "device_tree.dts"))
    if reconos_build_options.nfs_mount != "":
        nfs_mount = reconos_build_options.nfs_mount
    else:
        nfs_mount = "%s:%s" % (reconos_build_options.host_ip, reconos_build_options.nfs_root)
    sed_i([r's#\(nfsroot=[0-9.]\+\):[^,]*,tcp#nfsroot=%s,tcp#' % nfs_mount,
           r's#\bip=[0-9.]\+:#ip=%s:#' % reconos_build_options.board_ip],
           "device_tree.dts")
    long_operation(escape_for_shell([Path(ROOT, "linux-xlnx", "scripts", "dtc", "dtc"),
        "-I", "dts", "-O", "dtb", "-o", "device_tree.dtb", "device_tree.dts"]))



@task
def install_busybox():
    cd_verbose(ROOT, "busybox")
    make_parallel("CONFIG_PREFIX=%s" % ROOTFS, "install")

    tempfile_not_ignored = Path(ROOT, "busybox",  "busybox.links")
    tempfile_ignored     = Path(ROOT, "busybox", ".busybox.links")
    if tempfile_not_ignored.exists():
        # hide this file, so it doesn't make the git dirty
        # (unfortunately, it is not ignored in .gitignore)
        tempfile_not_ignored.move(tempfile_ignored)


def sed_backticks(expr, file, noprint = False):
    if isinstance(expr, (tuple, list)):
        exprs = map(escape_for_shell, expr)
        expr = "-e " + " -e ".join(exprs)
    else:
        expr = escape_for_shell(expr)
    file = escape_for_shell(file)
    if noprint:
        flags = "-n"
    else:
        flags = ""
    backticks('sed %s %s <%s' % (flags, expr, file))


def get_init_info(file, key):
    return sed_backticks(
        r'/^#\+\s*BEGIN INIT INFO\s*$/,/^#\+\s*END INIT INFO\s*$/ s/^#\s*%s:\s*\(.*\)$/\1/pi' % key,
        file,
        noprint=True)

def get_order(file, start_or_stop):
    order = get_init_info(file, "%s-order" % start_or_stop)
    if order:
        return order
    else:
        order = get_init_info(file, "order")

        if start_or_stop == "start":
            return order
        else:
            if order:
                return "%02u" % (100 - int(order))
            else:
                return ""

@task
def install_base_system():
    cd_verbose(ROOTFS)
    for dir in ["dev", "dev/pts",
            "etc", "etc/init.d", "etc/rc.d", "etc/default",
            "lib", "mnt", "opt", "proc", "root", "sys", "tmp",
            "var","var/run", "var/log"]:
        Path(ROOTFS, *dir.split("/")).mkdir()

    rc_script = Path("etc", "rcS")
    Path(FILES, "startup-script.sh").copy(rc_script)
    rc_script.chmod(0755)
    Path(FILES, "inittab").copy(Path("etc", "inittab"))
    Path(FILES, "fstab"  ).copy(Path("etc", "fstab"  ))

    for dir in ["init.d", "rc.d"]:
        Path(ROOTFS, "etc", dir).rmtree()
        Path(ROOTFS, "etc", dir).mkdir()

    for initd_script in glob.glob(Path(FILES, "init.d", "*")):
        src = Path(initd_script)
        name = src.name
        dst = Path(ROOTFS, "etc", "init.d", name)
        src.copy(dst)
        dst.chmod(0755)

        start_order = get_order(dst, "start")
        stop_order  = get_order(dst, "stop")

        if start_order:
            Path(ROOTFS, "etc", "rc.d", "S%s%s" % (start_order, name)).make_relative_link_to(dst)

        if stop_order:
            Path(ROOTFS, "etc", "rc.d", "K%s%s" % (start_order, name)).make_relative_link_to(dst)

    Path(ROOTFS, "etc", "default", "notify-host").write_file(
        "NOTIFY_HOST=" + escape_for_shell(reconos_build_options.host_ip) + "\n")


    Path(ROOTFS, "etc", "passwd").write_file(
        "root:x:0:0:root:/root:/bin/ash\n")
    Path(ROOTFS, "etc", "group").write_file(
        "root:x:0:\n")

    # see http://cross-lfs.org/view/clfs-sysroot/arm/cross-tools/eglibc.html
    Path(ROOTFS, "etc", "nsswitch.conf").write_file(heredoc("""
        passwd: files
        group: files
        shadow: files

        hosts: files dns
        networks: files

        protocols: files
        services: files
        ethers: files
        rpc: files
        """))
    
    Path("/etc/localtime").copy(Path(ROOTFS, "etc", "localtime"))

    # eglibc doc (see above) suggest that we add `/usr/local/lib` and `/opt/lib`,
    # but we don't use them
    touch(Path(ROOTFS, "etc", "ld.so.conf"))

@task
@needs("reconos_config")
def install_libc():
    # We do not copy all the locales (which are many times bigger than the whole rootfs), so we prepare
    # a list of the locales we do want.
    # Actually, there is more locale data in /usr/share/locale, but this is small, so we don't care.
    included_locales = ["en_US", "en_US.utf8", "de_DE.utf8"]
    locale_directories = map(lambda locale: "usr/lib/locale/"+locale, included_locales)

    # copy libc files
    libc_dir = get_libc_dir_after_reconos_config()
    sh("tar -c -C %s . --exclude=usr/lib/locale --exclude=usr/include | tar -x -C %s"
        % (escape_for_shell(libc_dir), escape_for_shell(ROOTFS)))
    sh("tar -c -C %s %s | tar -x -C %s"
        % (escape_for_shell(libc_dir), escape_for_shell(locale_directories), escape_for_shell(ROOTFS)))

@task
def install_dropbear():
    reconos_config()

    CROSS_COMPILE = os.environ["CROSS_COMPILE"]
    TOOLCHAIN_BIN_DIR = Path(CROSS_COMPILE).parent

    cd_verbose(ROOT, "dropbear")

    #TODO does this work?
    make_parallel(
        "PATH=%s" % escape_for_shell(TOOLCHAIN_BIN_DIR + ":" + os.environ["PATH"]),
        "prefix=%s" % ROOTFS,
        "install")

    ssh_dir = Path(ROOTFS, "root", ".ssh")
    authorized_keys = Path(ssh_dir, "authorized_keys")
    ssh_dir.mkdir(parents=True)
    public_key_file = Path("~/.ssh/id_rsa.pub").expand_user()
    if public_key_file.exists() and not authorized_keys.exists():
        public_key = public_key_file.read_file()
        authorized_keys.write_file(public_key)

        ssh_dir.chmod(0755)
        authorized_keys.chmod(0600)

    shells_file = Path(ROOTFS, "etc", "shells")
    shells = ""
    if shells_file.exists():
        shells = shells_file.read_file()
    if not re.match("^/bin/ash$", shells):
        shells += "/bin/ash\n"
        shells_file.write_file(shells)

@task
def install_kernel_modules():
    cd_verbose(ROOT, "linux-xlnx")
    make_parallel(
        "INSTALL_MOD_PATH=%s" % ROOTFS,
        "modules_install")

@task
def install_reconos():
    KERNEL_RELEASE=Path(ROOT, "linux-xlnx", "include", "config", "kernel.release").read_file().strip()
    RECONOS_MODULE="kernel/drivers/mreconos.ko"
    MODULE_DIR=Path(ROOTFS, "lib", "modules", KERNEL_RELEASE)
    src = Path(ROOT, "reconos", "linux", "driver", "mreconos.ko")
    dst = Path(MODULE_DIR, *RECONOS_MODULE.split("/"))
    src.copy(dst)
    append_file(Path(MODULE_DIR, "modules.dep"),
        RECONOS_MODULE + ": \n")

@task
@needs("reconos_config", "install_busybox", "install_base_system",
    "install_libc", "install_dropbear", "install_kernel_modules",
    "install_reconos")
def build_rootfs():
    #TODO: Path(ROOTFS).mkdir(parents=True)
    pass

@task
@cmdoptsgroup("reconos_build")
def update_nfsroot():
    sh("%s mkdir -p %s" % (sudo_cmd(), escape_for_shell(reconos_build_options.nfs_root)))
    sh("%s cp -a %s %s" % (sudo_cmd(), escape_for_shell(Path(ROOTFS, ".")), escape_for_shell(reconos_build_options.nfs_root)))
    
    exclude = [".ash_history", "dropbear", "motd", "log", "run", "lost+found"]
    exclude_pattern = "/\\(" + "\\|".join(map(escape_for_regex, exclude)) + "\\)\\(/\|$\\)"
    exclude_filter = "grep -v " + escape_for_shell(exclude_pattern)
    # We could use <(...) syntax, but sh doesn't support it, so we use temporary files instead.
    def sudo_list_files(dir):
        _, tmpfile = tempfile.mkstemp()
        cmd = "cd %s ; find | %s | sort >%s" % (escape_for_shell(dir), exclude_filter, escape_for_shell(tmpfile))
        sh("%s sh -c %s" % (sudo_cmd(), escape_for_shell(cmd)))
        return tmpfile

    file_list1 = None
    file_list2 = None
    try:
        file_list1 = sudo_list_files(ROOTFS)
        file_list2 = sudo_list_files(reconos_build_options.nfs_root)
        res = sh_test("%s diff -y --suppress-common-lines %s %s"
            % (sudo_cmd(), escape_for_shell(file_list1), escape_for_shell(file_list2)))
        if res != 0:
            logger.warn("WARNING: NFS root directory %s contains additional files!" % reconos_build_options.nfs_root)
    finally:
        if file_list1:
            Path(file_list1).remove()
        if file_list2:
            Path(file_list2).remove()

    authorized_keys_file = Path(reconos_build_options.nfs_root, "root", ".ssh", "authorized_keys")
    ssh_dirs_and_files = [ authorized_keys_file.ancestor(i) for i in range(3) ]
    if authorized_keys_file.exists():
        sh("%s chown root:root %s" % (sudo_cmd(), escape_for_shell(ssh_dirs_and_files)))
        if reconos_build_options.nfs_no_sudo:
            st = os.stat(authorized_keys_file)
            if st.st_uid != 0 or st.st_gid != 0:
                logger.error(heredoc("""
                    ERROR: The authorized_keys file must belong to root, but it doesn't. We won't
                           be able to connect to the board. Please make sure that the NFS options
                           are correct (see help of --nfs-no-sudo).
                    """))
                sys.exit(1)
        for file_or_dir in ssh_dirs_and_files:
            if file_or_dir.isdir():
                rights = "700"
            else:
                rights = "600"
            sh("%s chmod %s %s" % (sudo_cmd(), rights, escape_for_shell(file_or_dir)))

def sudo_append_to(file, *lines):
    text = "\n".join(lines) + "\n"
    sh(ShellEscaped("echo %s | %s tee -a %s >/dev/null") % (text, sudo_cmd(), file))

@task
@cmdoptsgroup("reconos_build")
@needs("check_host_ip", "reconos_config", "update_nfsroot")
def boot_zynq():
    if reconos_build_options.nfs_mount == "":
        if reconos_build_options.nfs_no_sudo:
            logger.warn("WARNING: You have set --nfs-no-sudo, but not --nfs-mount. I will now try to create the "
                + "NFS share without using sudo. This will most likely fail!")

        # create NFS share for the board
        sudo_modify_by("/etc/exports",
            ShellEscaped("grep -v %s") % reconos_build_options.nfs_root)
        sudo_append_to("/etc/exports",
            "%s %s(rw,no_root_squash,no_subtree_check)" % (reconos_build_options.nfs_root, reconos_build_options.board_ip))
        sh("%s exportfs -rav" % sudo_cmd())

    #TODO this should be in git...
    edk_project_dir = Path(ROOT, "reconos", "demos", reconos_build_options.demo, "hw", "edk_zynq_linux")
    Path(FILES, "ps7_init.tcl").copy(Path(edk_project_dir, "ps7_init.tcl"))

    # The Digilent driver installer makes ~/.cse owned by root (or probably we shouldn't
    # have run it with sudo). xmd tries to put a temporary file into it and fails. Therefore,
    # we fix this.
    cse_dir = Path("~/.cse").expand_user()
    if cse_dir.exists() and cse_dir.stat().st_uid == 0:
        if reconos_build_options.nfs_no_sudo:
            logger.error("~/.cse is owned by root. Please run: sudo chown -R \"$USER\" ~/.cse")
            sys.exit(1)
        else:
            sh('sudo chown -R "$USER" ~/.cse')

    #NOTE If this fails because it doesn't find the cable, install the cable drivers:
    #     cd $XILINX/bin/lin64/digilent ; ./install_digilent.sh
    cd_verbose(ROOT)
    sh(escape_for_shell([
        "bash",
        "zynq_boot_jtag.sh",
        Path(ROOT, "linux-xlnx", "arch", "arm", "boot", "uImage"),
        Path(edk_project_dir, "device_tree.dtb"),
        Path(edk_project_dir, "ps7_init.tcl"),
        Path(ROOT, "u-boot-xlnx", "u-boot")
        ]))

    bitfile = Path(edk_project_dir, "implementation", "system.bit")
    run_xmd("fpga -f %s" % escape_for_shell(bitfile))

    logger.info("The board should be booting, now.")
    logger.info("If you abort u-boot for some reason, enter 'jtagboot' or 'bootm 0x3000000 - 0x2A00000'.")


    # wait for the board to contact us
    if sh_test(ShellEscaped("timeout 30 nc -l %s 12342") % reconos_build_options.host_ip) != 0:
        logger.error(
            "ERROR: The board didn't send the 'done' message within 30 seconds.")

        if not can_ping(reconos_build_options.host_ip):
            logger.error(heredoc("""
                ERROR: I cannot ping %s, so I think for some reason your IP got reset.
                  Please make sure that you use an ethernet switch (i.e. don't
                  directly connect your PC to the board) or make sure that the IP
                  settings survive a reset of the ethernet connection (i.e. set them
                  in NetworkManager or similar).
                """ % (reconos_build_options.host_ip)))
            sys.exit(1)
        if not can_ping(reconos_build_options.board_ip):
            logger.warn(
                "WARNING: I cannot ping the board (%s). This is a bad sign.", reconos_build_options.board_ip)

        sys.exit(1)

def get_board_pubkey(keytype):
    infofile = Path(reconos_build_options.nfs_root, "etc", "dropbear", "dropbear_%s_host_key.info" % keytype)
    text = infofile.read_file()
    return re.search("^(ssh-\S+\s+\S+)", text, re.MULTILINE).group(1)

def pubkey_is_in_known_hosts(hostname, pubkey):
    return 0 == sh_test("ssh-keygen -F %s | grep -q %s" %
        (escape_for_shell(hostname), escape_for_shell(pubkey)))

@task
@cmdoptsgroup("reconos_build")
def prepare_ssh():
    pubkey = get_board_pubkey("rsa")

    if pubkey and not pubkey_is_in_known_hosts(reconos_build_options.board_ip, pubkey):
        # board key is not in known_hosts, so we remove any old ones and add the new one
        sh("ssh-keygen -R %s" % reconos_build_options.board_ip)
        append_file(
            Path("~", ".ssh", "known_hosts").expand_user(),
            "%s %s" % (reconos_build_options.board_ip, escape_for_shell(pubkey)))
        # re-hash the file
        sh("ssh-keygen -H")

def ssh_zynq_special(command, redirects=""):
    sh("ssh -o PubkeyAuthentication=yes root@%s %s %s"
        % (reconos_build_options.board_ip, escape_for_shell(command), redirects))

def ssh_zynq(*command):
    return ssh_zynq_special(command)


@task
@cmdoptsgroup("reconos_build")
@needs("check_host_ip", "prepare_ssh")
def ssh_shell():
    ssh_zynq()

@task
@cmdoptsgroup("reconos_build")
@needs("check_host_ip", "prepare_ssh")
def run_demo():
    demo_args = from_env("DEMO_ARGS", "2 2 50")

    ssh_zynq_special("cat >%s" % escape_for_shell(Path("/tmp", reconos_build_options.demo)),
        "<%s" % escape_for_shell(Path(ROOT, "reconos", "demos", reconos_build_options.demo, "linux", reconos_build_options.demo)))
    ssh_zynq("chmod +x /tmp/%s && /tmp/%s %s" % (reconos_build_options.demo, reconos_build_options.demo, demo_args))

@task
@needs("build", "update_nfsroot", "boot_zynq", "run_demo")
def build_and_run():
    pass

@task
@needs("clean, build_and_run")
def clean_build_and_run():
    pass
