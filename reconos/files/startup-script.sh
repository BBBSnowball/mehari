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
