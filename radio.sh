#!/bin/bash

export PATH=$PATH:/usr/local/bin

rtp_port=1500
rtp_host=224.0.1.23
rtp_cert=

while [ $# -gt 0 ]
do
    case "$1" in
        -h)  rtp_host="$2"; shift;;
        -p)  rtp_port="$2"; shift;;
        -c)  rtp_cert="$2"; shift;;
        --)  shift; break;;
        -*)
            echo >&2 \
            "usage: $0 [-h rtp_host] [-p rtp_port] [-c rtp_cert] http_host http_port"
            exit 1;;
        *)  break;;     # terminate while loop
    esac
    shift
done

while true; do
   print -n "GET / HTTP/1.0\r\n\r\n" | nc $1 $2 | \
            buffer | \
               (if [ $rtp_cert ]; then
                   ./poc -c $rtp_cert -p $rtp_port -s $rtp_host -
                else
                   ./poc -p $rtp_port -s $rtp_host -
                fi;)

   print "Reconnecting..."
   sleep 120
done
