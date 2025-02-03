#!/usr/bin/env bash

port=$1
if [ -z "$port" ]; then
    port=1234
fi

# reference: https://mritd.com/2022/02/06/clash-tproxy/

set -ex

sysctl net.ipv4.conf.all.forwarding=1
sysctl net.ipv4.conf.eth0.rp_filter=0

# skip local
iptables -t mangle -N dual_skip

iptables -t mangle -A dual_skip -d 0.0.0.0/8 -j ACCEPT
iptables -t mangle -A dual_skip -d 127.0.0.0/8 -j ACCEPT
iptables -t mangle -A dual_skip -d 10.0.0.0/8 -j ACCEPT
iptables -t mangle -A dual_skip -d 172.16.0.0/12 -j ACCEPT
iptables -t mangle -A dual_skip -d 192.168.0.0/16 -j ACCEPT
iptables -t mangle -A dual_skip -d 169.254.0.0/16 -j ACCEPT
iptables -t mangle -A dual_skip -d 224.0.0.0/4 -j ACCEPT
iptables -t mangle -A dual_skip -d 240.0.0.0/4 -j ACCEPT

iptables -t mangle -A PREROUTING -j dual_skip

# mark tcp
iptables -t mangle -N dual_mark

iptables -t mangle -A dual_mark -j MARK --set-mark 1
iptables -t mangle -A dual_mark -j ACCEPT

iptables -t mangle -A PREROUTING -p tcp -m socket -j dual_mark

# redirect tproxy
iptables -t mangle -N dual_proxy

iptables -t mangle -A dual_proxy -p tcp -j TPROXY --tproxy-mark 0x1/0x1 --on-port $port --on-ip 127.0.0.1

iptables -t mangle -A PREROUTING -j dual_proxy

# route table
ip -f inet rule add fwmark 1 lookup 100
ip -f inet route add local 0.0.0.0/0 dev lo table 100
