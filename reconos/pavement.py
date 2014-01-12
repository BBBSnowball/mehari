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


from paver.easy import task, needs, cmdopts, might_call
import paver
#TODO use path.py that is bundled with paver
from unipath import Path
import sys, os, re, subprocess, glob, tempfile, logging

ROOT   = Path(__file__).absolute().parent
FILES  = Path(ROOT, "files")
ROOTFS = Path(ROOT, "_install")

logger = logging.getLogger(__name__)
logging.basicConfig()
if paver.tasks.environment.verbose:
    loglevel = logging.DEBUG
else:
    loglevel = logging.WARNING
logger.setLevel(loglevel)
logging.getLogger().setLevel(loglevel)

sys.path.insert(1, Path(ROOT.parent, "tools", "python-helpers"))
from mehari.build_utils import *


#TODO use commandline arguments provided by paver
def global_from_env(name, default = None):
    globals()[name] = from_env(name, default)

##global_from_env("ZYNQ_BOARD_REVISION", "d")
global_from_env("MAKE_PARALLEL_PROCESSES", "4"            )
global_from_env("HOST_IP",                 "192.168.24.17")
global_from_env("BOARD_IP",                "192.168.24.23")
global_from_env("DEMO",                    "sort_demo"    )
global_from_env("NFS_ROOT",                "/nfs/zynqn"   )

if False:
    # make lint happy...
    MAKE_PARALLEL_PROCESSES = None
    HOST_IP = None
    BOARD_IP = None
    DEMO = None
    NFS_ROOT = None

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
    return os.system('ifconfig | grep -q "inet addr6\?:%s[/ ]"' % ip) == 0

def can_ping(host):
    return os.system('ping -c1 "%s" >/dev/null' % host) == 0

@task
def check_host_ip():
    "make sure this host responds to HOST_IP"
    if not host_has_ip(HOST_IP) or not can_ping(HOST_IP):
        logger.error(heredoc("""
            ERROR: HOST_IP is %s, but none of your interfaces has that IP.
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
                TODO: HOST_IP=... BOARD_IP=... $0
              - only build and don't run it:
                TODO: BOOT_ZYNQ=0 $0
            """ % (HOST_IP, HOST_IP, HOST_IP)))
        sys.exit(1)

@task
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
def check():
    check_submodules()
    check_libc()
    check_host_ip()


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
    global MAKE_PARALLEL_PROCESSES
    long_operation(("make -j%s " % MAKE_PARALLEL_PROCESSES) + escape_for_shell(targets))


def sudo_modify_by(file, *command):
    x, tmpfile = tempfile.mkstemp()
    Path(file).copy(tmpfile)
    sh(ShellEscaped("%s <%s | sudo tee %s >/dev/null")
        % (command, tmpfile, file))
    Path(tmpfile).remove()

def run_xmd(commands):
    commands = escape_for_shell(str(commands) + " ; exit")
    sh("echo %s | xmd" % commands)

def run_xps(project, commands):
    cmd = ShellEscaped("echo %s | xps -nw %s") % (str(commands) + " ; exit", project)
    # try 3 times
    for i in range(3):
        res = os.system(cmd)
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
    logger.info("In directory: " + str(dir))
    Path(*dir).chdir()

def sed_i(expr, file):
    if isinstance(expr, (tuple, list)):
        exprs = map(escape_for_shell, expr)
        expr = "-e " + " -e ".join(exprs)
    else:
        expr = escape_for_shell(expr)
    file = escape_for_shell(file)
    sh('sed -i %s %s' % (expr, file))

@task

@needs("reconos_config")
def build_uboot():
    if "CROSS_COMPILE" not in os.environ:
        raise Exception("reconos_config didn't set CROSS_COMPILE")

    cd_verbose(ROOT, "u-boot-xlnx")
    sh(r'patch -N -p1 <"%s/u-boot-xlnx-zynq.patch"' % (FILES,))

    sed_i([r's/^#define CONFIG_IPADDR\b.*$/#define CONFIG_IPADDR   %s/'   % BOARD_IP,
           r's/^#define CONFIG_SERVERIP\b.*$/#define CONFIG_SERVERIP %s/' % HOST_IP],
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
@needs("reconos_config")
def build_reconos():
    cd_verbose(ROOT, "reconos", "linux", "driver")
    make_parallel()
    cd_verbose(ROOT, "reconos", "linux", "lib")
    make_parallel()

    cd_verbose(ROOT, "reconos", "demos", DEMO, "linux")
    make_parallel()

    cd_verbose(ROOT, "reconos", "demos", DEMO, "hw")

    xil_version = xilinx_version()
    #TODO get design for 14.7 from Christoph
    #sed -i 's/zedboard_minimal_[0-9].*/zedboard_minimal_'"$XILINX_VERSION"'/' setup_zynq
    logger.info("Not setting version")  #DEBUG

    sh("bash reconos_setup.sh setup_zynq")

    cd_verbose(ROOT, "reconos", "demos", DEMO, "hw", "edk_zynq_linux")
    #TODO keep __xps folder in git and use `make -f system.make bits`
    #TODO This sometimes fails with a segfault (prints "Segmentation fault" and
    #     exits with status 35584). We should run it again in that case.
    long_operation(run_xps_cmd("system", "run bits"))

    Path(FILES, "device_tree.dts").copy(Path(".", "device_tree.dts"))
    sed_i([r's#\(nfsroot=[0-9.]\+\):[^,]*,tcp#nfsroot=%s:%s,tcp#' % (HOST_IP, NFS_ROOT),
           r's#\bip=[0-9.]\+:#ip=%s:#' % BOARD_IP],
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
        "NOTIFY_HOST=" + escape_for_shell(HOST_IP) + "\n")


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
def install_libc():
    config = reconos_config()

    # copy libc files
    #libc_dir = config["libc_dir"]  #TODO return value of task is always None :-(
    libc_dir = get_libc_dir_after_reconos_config()
    for dir in ["usr/bin", "lib", "usr/lib", "usr/share", "sbin", "usr/sbin"]:
        dir_components = dir.split("/")
        src = Path(libc_dir, *dir_components)
        dst = Path(ROOTFS,   *dir_components)
        copy_tree(src, dst)

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
def update_nfsroot():
    sh("sudo mkdir -p %s" % escape_for_shell(NFS_ROOT))
    sh("sudo cp -a %s %s" % (escape_for_shell(Path(ROOTFS, ".")), escape_for_shell(NFS_ROOT)))
    
    exclude = [".ash_history", "dropbear", "motd", "log", "run"]
    exclude_args = " ".join(map(lambda x: "-x " + escape_for_shell(x), exclude))
    res = os.system("sudo diff -r %s %s %s"
        % (escape_for_shell(ROOTFS), escape_for_shell(NFS_ROOT), exclude_args))
    if res != 0:
        logger.warn("WARNING: NFS root directory %s contains additional files!" % NFS_ROOT)

    authorized_keys_file = Path(NFS_ROOT, "root", ".ssh", "authorized_keys")
    ssh_dirs_and_files = [ authorized_keys_file.ancestor(i) for i in range(3) ]
    if authorized_keys_file.exists():
        sh("sudo chown root:root %s" % escape_for_shell(ssh_dirs_and_files))
        for file_or_dir in ssh_dirs_and_files:
            if file_or_dir.isdir():
                rights = "700"
            else:
                rights = "600"
            sh("sudo chmod %s %s" % (rights, escape_for_shell(file_or_dir)))

def sudo_append_to(file, *lines):
    text = "\n".join(lines) + "\n"
    sh(ShellEscaped("echo %s | sudo tee -a %s >/dev/null") % (text, file))

@task
@needs("check_host_ip", "reconos_config", "update_nfsroot")
def boot_zynq():
    sudo_modify_by("/etc/exports",
        ShellEscaped("grep -v %s") % NFS_ROOT)
    sudo_append_to("/etc/exports",
        "%s %s(rw,no_root_squash,no_subtree_check)" % (NFS_ROOT, BOARD_IP))
    sh("sudo exportfs -rav")

    #TODO this should be in git...
    edk_project_dir = Path(ROOT, "reconos", "demos", DEMO, "hw", "edk_zynq_linux")
    Path(FILES, "ps7_init.tcl").copy(Path(edk_project_dir, "ps7_init.tcl"))

    # The Digilent driver installer makes ~/.cse owned by root (or probably we shouldn't
    # have run it with sudo). xmd tries to put a temporary file into it and fails. Therefore,
    # we fix this.
    cse_dir = Path("~/.cse").expand_user()
    if cse_dir.exists() and cse_dir.stat().st_uid == 0:
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

    print("The board should be booting, now.")
    print("If you abort u-boot for some reason, enter 'jtagboot' or 'bootm 0x3000000 - 0x2A00000'.")


    # wait for the board to contact us
    if os.system(ShellEscaped("timeout 30 nc -l %s 12342") % HOST_IP) != 0:
        logger.error(
            "ERROR: The board didn't send the 'done' message within 30 seconds.")

        if not can_ping(HOST_IP):
            logger.error(heredoc("""
                ERROR: I cannot ping %s, so I think for some reason your IP got reset.
                  Please make sure that you use an ethernet switch (i.e. don't
                  directly connect your PC to the board) or make sure that the IP
                  settings survive a reset of the ethernet connection (i.e. set them
                  in NetworkManager or similar).
                """ % (HOST_IP)))
            sys.exit(1)
        if not can_ping(BOARD_IP):
            logger.warn(
                "WARNING: I cannot ping the board (%s). This is a bad sign.", BOARD_IP)

        sys.exit(1)

def get_board_pubkey(keytype):
    infofile = Path(NFS_ROOT, "etc", "dropbear", "dropbear_%s_host_key.info" % keytype)
    text = infofile.read_file()
    return re.search("^(ssh-\S+\s+\S+)", text, re.MULTILINE).group(1)

def pubkey_is_in_known_hosts(hostname, pubkey):
    return 0 == os.system("ssh-keygen -F %s | grep -q %s" %
        (escape_for_shell(hostname), escape_for_shell(pubkey)))

@task
def prepare_ssh():
    pubkey = get_board_pubkey("rsa")

    if pubkey and not pubkey_is_in_known_hosts(BOARD_IP, pubkey):
        # board key is not in known_hosts, so we remove any old ones and add the new one
        sh("ssh-keygen -R %s" % BOARD_IP)
        append_file(
            Path("~", ".ssh", "known_hosts").expand_user(),
            "%s %s" % (BOARD_IP, escape_for_shell(pubkey)))
        # re-hash the file
        sh("ssh-keygen -H")

def ssh_zynq_special(command, redirects=""):
    sh("ssh -o PubkeyAuthentication=yes root@%s %s %s"
        % (BOARD_IP, escape_for_shell(command), redirects))

def ssh_zynq(*command):
    return ssh_zynq_special(command)


@task
@needs("check_host_ip", "prepare_ssh")
def ssh_shell():
    ssh_zynq()

@task
@needs("check_host_ip", "prepare_ssh")
def run_demo():
    demo_args = from_env("DEMO_ARGS", "2 2 50")

    ssh_zynq_special("cat >%s" % escape_for_shell(Path("/tmp", DEMO)),
        "<%s" % escape_for_shell(Path(ROOT, "reconos", "demos", DEMO, "linux", DEMO)))
    ssh_zynq("chmod +x /tmp/%s && /tmp/%s %s" % (DEMO, DEMO, demo_args))

@task
@needs("build", "update_nfsroot", "boot_zynq", "run_demo")
def build_and_run():
    pass

@task
@needs("clean, build_and_run")
def clean_build_and_run():
    pass
