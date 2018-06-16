#!/usr/bin/env bash

if [[ $# > 1 ]]; then
    echo "Usage: ./restart-varnish.sh /tmp/widgets/noop.vcl"
    exit 1
fi

# Kill existing varnishd processes
(! pgrep varnishd &> /dev/null) || (sudo killall -q varnishd && sleep 2)

if [[ ! -z $1 ]]; then
    VCL="-f $1"
fi

# Start varnishd
cd "${BASH_SOURCE%/*}"
sudo varnishd -a :80 -a :9090 "$VCL "-p vcc_allow_inline_c=on
