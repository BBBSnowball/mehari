#!/bin/sh

echo "Starting rcS..."

for x in /etc/rc.d/S* ; do
	"$x" start
done

echo "rcS Complete"

#TODO ntpd -p 1.europe.pool.ntp.org &
