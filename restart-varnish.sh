#!/usr/bin/env bash

if [[ $# > 1 ]]; then
    echo "Usage: ./restart-varnish.sh /tmp/widgets/noop.vcl"
    exit 1
fi

# Kill existing varnishd processes
(! pgrep varnishd &> /dev/null) || (sudo killall -q varnishd && sleep 2)

if [[ -z $1 ]]; then
    VCL="-b 10.0.0.2"
else
    VCL="-f $1"
fi

# Start varnishd
sudo varnishd -a 192.168.1.1:80 -a 192.168.1.1:9090 $VCL -p vcc_allow_inline_c=on
