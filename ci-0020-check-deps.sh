#!/bin/bash

# We must run this script with bash because the settings64.sh of Xilinx doesn't work with dash.

set -e

if [ "$(uname -m)" != "x86_64" ] ; then
	echo "Please use a 64-bit x86 machine. It may work on 32-bit x86. Feel free to try it (and please report the results)." >&2
	exit 1
fi

check_program() {
	if ! which "$1" >/dev/null ; then
		if [ -n "$3" ] ; then
			echo "Please install $3" >&2
		else
			echo "Please install $1" >&2
		fi
		if [ -n "$2" ] ; then
			echo "On Debian: apt-get install $2" >&2
		fi
		exit 1
	fi
}

check_python() {
	if ! python -c "import $1" ; then
		echo "Please install the $1 library for Python" >&2
		echo "On Debian: apt-get install $2" >&2
		exit 1
	fi
}

check_program bc         bc               "the 'bc' util (used by Kernel make system)"
check_program python     python2.7        "Python 2.7 (newer versions MIGHT work)"
check_program git        git-core
check_program ssh        openssh-client
check_program ssh-keygen openssh-client
#check_program exportfs   nfs-kernel-server		# not always necessary...

check_program ifconfig
check_program grep
check_program ping
check_program sed
check_program find
check_program tee
check_program patch
check_program touch
check_program diff
check_program gawk
check_program curl
check_program tar
check_program cut
check_program wc
check_program realpath

if ! which make >/dev/null || ! which gcc >/dev/null || ! which ld >/dev/null ; then
	echo -n "We need a working build system. This usually means gcc, make, binutils and " >&2
	echo -n "some headers and libraries. Do NOT only install enough tools to make this "  >&2
	echo    "warning go away! You need something like the build-essential in Debian."     >&2
	echo    "On Debian: apt-get install build-essential"                                  >&2
	echo
	echo -n "This is about the compiler for the host system. The cross-compiler for "     >&2
	echo    "the target system is part of the Xilinx tools."                              >&2
	exit
fi

if ! which gmake >/dev/null ; then
	echo "Xilinx needs gmake as an alias for the GNU make executable. Please run:" >&2
	echo "  sudo ln -s `which make` /usr/bin/gmake"                                >&2
	exit
fi

if ! find --version | grep -q "GNU findutils" ; then
	# Windows has a totally different program called 'find'. I'm not sure about Mac OS.
	echo "Please install GNU findutils and make sure we get its version of find." >&2
	exit 1
fi

if ! sed --version | grep -q "^GNU sed version" ; then
	# Mac OS has BSD tools by default and GNU tools have a 'g' prefix when installed by macports.
	echo "Please install a GNU sed and make sure we get it, when we call 'sed' (not gsed)." >&2
	exit 1
fi
if ! make --version | grep -q "^GNU Make [0-9]" ; then
	# Mac OS has BSD tools by default and GNU tools have a 'g' prefix when installed by macports.
	# Other systems may also have an incompatible make implementation, e.g. nmake on Windows.
	echo "Please install a GNU make and make sure we get it, when we call 'make' (not gmake)." >&2
	exit 1
fi

check_python unipath python-unipath
check_python yaml    python-yaml

if [ -z "$XILINX_SETTINGS_SCRIPT" ] ; then
	source ./ci-*-find-xilinx-tools.sh
fi
if [ ! -e "$XILINX_SETTINGS_SCRIPT" ] ; then
	echo -n "I cannot find the Xilinx ISE tools. Please install version 14.6 or 14.7 (other versions MIGHT work). " >&2
	echo    "If Xilinx ISE is installed, please help me find it. You can either:" >&2
	echo    "- set XILINX_VERSION, if it lives in /opt/Xilinx/$XILINX_VERSION" >&2
	echo    "- set XILINX=/path/to/Xilinx/ISE_DS/ISE" >&2
	echo    "- set XILINX_SETTINGS_SCRIPT=/path/to/Xilinx/ISE_DS/settings64.sh" >&2
	exit 1
fi

if ! ( . "$XILINX_SETTINGS_SCRIPT" >/dev/null ; xmd -help >/dev/null ) ; then
	echo -n "I cannot execute the xmd program. That probably means that dynamic libraries are missing. " >&2
	echo    "Please make sure that you have a 32-bit libc6." >&2
	echo    "On Debian, you should install libc6-i386 (or ia32-libs)." >&2
	exit 1
fi

echo -n "I couldn't find any required tool or library that is missing. However, this script doesn't check "
echo    "every dependency, so something could be missing. Please report any omission that you find."
echo

echo -n "In particular, I haven't checked for any headers and libraries that we need. Most build scripts will "
echo -n "do that themselves. Those files are usually in a package called something-dev or something-devel. You "
echo    "will need headers and libraries for:"
echo    "- Python (python2.7-dev)"
echo    "- ctemplate (libctemplate-dev)"
echo
# Furthermore, the build scripts are much more thorough than a simple check in this could be, so we would
# have lot's of false positives.
