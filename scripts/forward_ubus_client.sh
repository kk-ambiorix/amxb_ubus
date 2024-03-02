#!/bin/sh -e

if [ $# != 2 ]; then
    echo "usage: $0 <unix domain socket> <address:port>"
    echo "example: $0 /var/run/ubus/ubus.sock 192.168.1.1:1971"
    exit 0
fi

PRIVATE_SOCKET=$1
PUBLIC_IP=$2
SOCAT_LOG=/tmp/socat-client-$PUBLIC_IP.log

echo "--> socat relay from UNIX domain [$PRIVATE_SOCKET] to socket [$PUBLIC_IP]"
echo "--> logs: $SOCAT_LOG"

socat -d -d -lf $SOCAT_LOG UNIX-LISTEN:$PRIVATE_SOCKET,fork TCP:$PUBLIC_IP
