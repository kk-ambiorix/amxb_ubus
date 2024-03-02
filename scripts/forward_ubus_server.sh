#!/bin/sh -e

if [ $# != 2 ]; then
    echo "usage: $0 <port> <unix domain socket>"
    echo "example: $0 1971 /var/run/ubus/ubus.sock"
    exit 0
fi

PUBLIC_PORT=$1
PRIVATE_SOCKET=$2
SOCAT_LOG=/tmp/socat-$PUBLIC_PORT.log

echo "--> socat relay from port [$PUBLIC_PORT] to socket [$PRIVATE_SOCKET]"
echo "--> logs: $SOCAT_LOG"

socat -d -d -lf $SOCAT_LOG TCP4-LISTEN:$PUBLIC_PORT,reuseaddr,fork UNIX-CONNECT:$PRIVATE_SOCKET
