#!/bin/bash

# We have to use bash because the Xilinx settings script won't work with dash.

set -e

if [ -n "$XILINX_SETTINGS_SCRIPT" ] ; then
	# do nothing
	echo -n
elif [ -n "$XILINX" ] ; then
	XILINX_SETTINGS_SCRIPT="$(dirname "$XILINX")/settings64.sh"
else
	if [ -z "$XILINX_VERSION" ] ; then
		XILINX_VERSION="14.6"
	fi
	XILINX_SETTINGS_SCRIPT="/opt/Xilinx/$XILINX_VERSION/ISE_DS/settings64.sh"
fi

if [ -e "$XILINX_SETTINGS_SCRIPT" ] ; then
	if [ -n "$ENVVAR_STORE" ] ; then
		echo "export XILINX_SETTINGS_SCRIPT='$XILINX_SETTINGS_SCRIPT'" >>"$ENVVAR_STORE"

		if [ -z "$XILINX" ] ; then
			XILINX="$(. "$XILINX_SETTINGS_SCRIPT" >/dev/null ; echo "$XILINX")"
		fi
		echo "export XILINX='$XILINX'" >>"$ENVVAR_STORE"

		if [ -z "$XILINX_VERSION" ] ; then
			XILINX_VERSION=$(. "$XILINX_SETTINGS_SCRIPT" >/dev/null ; xps -help|sed -n 's/^Xilinx EDK \([0-9]\+\.[0-9]\+\)\b.*$/\1/p')
			if ! echo "$XILINX_VERSION" | grep -q "^[0-9]\+\.[0-9]\+$" ; then
				echo "ERROR: I couldn't find out the version of the Xilinx tools. Please set \$XILINX_VERSION." >&2
				echo "       My guess is '$XILINX_VERSION', but that doesn't seem to be right." >&2
				exit 1
			fi
		fi
		echo "export XILINX_VERSION='$XILINX_VERSION'" >>"$ENVVAR_STORE"
	fi
else
	echo "WARNING: Xilinx Tools not found. ci-*-check-deps.sh will report this error." >&2
	echo "My guess for the setting script location is: $XILINX_SETTINGS_SCRIPT"        >&2
	XILINX_SETTINGS_SCRIPT="-"
	if [ -n "$ENVVAR_STORE" ] ; then
		echo "export XILINX_SETTINGS_SCRIPT='-'" >>"$ENVVAR_STORE"
	fi
fi
