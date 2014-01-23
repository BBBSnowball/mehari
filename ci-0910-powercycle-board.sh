#!/bin/bash

# Test how long it takes for the board to turn off resp. on when we switch power. This measures how
# long it takes for the JTAG interface to come up. Other things may be slower or faster.
#   sispmctl -f 1 ; while   lsusb|grep -q 0403:6014 ; do echo -n . ; sleep 1 ; done ; echo
#   sispmctl -o 1 ; while ! lsusb|grep -q 0403:6014 ; do echo -n . ; sleep 1 ; done ; echo
# -> about 3 seconds each
# We wait longer than that (10 seconds) to make sure the board is really reset. I don't have any
# evidence that supports this number, but I think it is reasonable.

if [ -n "$SISPM_OUTLET" ] ; then
    #TODO make sure that the board doesn't start, i.e. delete the kernel from tftp and probably also
    #     wreck the rootfs
    echo "Turning power to the board off and on again..."
    sispmctl -f "$SISPM_OUTLET"
    sleep 10
    sispmctl -o "$SISPM_OUTLET"
    sleep 10
else
    echo "WARN: You don't have a way to powercycle the board before the test. We recommand that you" \
         "get a Gembird SilverShield USB-controlled Programmable Power Outlet (or something similar - but" \
         "this is the only one we support). You have to tell us to which outlet the board is connected, for" \
         "example: connect the board to the first outlet and set the environment variable SISPM_OUT='1'."
fi
