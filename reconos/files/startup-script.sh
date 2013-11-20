#!/bin/sh

HOST_IP=""
SYSLOG_OPTIONS=

echo "Starting rcS..."

mkdir -p /var/run /var/log

echo "++ Mounting filesystem"
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
mount -t tmpfs none /var/run

ttydev=`cat /sys/class/tty/ttyPS0/dev`
ttymajor=${ttydev%%:*}
ttyminor=${ttydev##*:}
if [ -c /dev/ttyPS0 ]
then
	rm /dev/ttyPS0
fi

mknod /dev/ttyPS0 c $ttymajor $ttyminor


echo "++ Starting syslog"

syslogd -O /var/log/messages $SYSLOG_OPTIONS


echo "++ Starting Dropbear SSH server"

mkdir -p /etc/dropbear

generate_key_if_it_doesnt_exist() {
	keytype="$1"
	if [ ! -e "/etc/dropbear/dropbear_${keytype}_host_key" ] ; then
		dropbearkey -t "${keytype}" -f "/etc/dropbear/dropbear_${keytype}_host_key" \
			| tee "/etc/dropbear/dropbear_${keytype}_host_key.info"
	fi
}

generate_key_if_it_doesnt_exist dss
generate_key_if_it_doesnt_exist rsa

echo "Welcome to Zynq"  >/etc/motd
echo -n "Kernel: "     >>/etc/motd
uname -a               >>/etc/motd

if [ -e "~root/.ssh/authorized_keys" ] ; then
	chown root:root "~root/.ssh" "~root/.ssh/authorized_keys"
fi

dropbear -s -p 22 -p 2222 -b /etc/motd


if [ -n "$HOST_IP" ] ; then
	echo "++ Telling $HOST_IP that the SSH server is available"
	echo "done" | nc "$HOST_IP" 12342 -w 10 || true
fi


echo "rcS Complete"

#TODO ntpd -p 1.europe.pool.ntp.org
