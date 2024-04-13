#!/usr/bin/env bash

# reference: https://mritd.com/2022/02/06/clash-tproxy/

set -ex

sysctl -w net.ipv4.ip_forward=1

# ROUTE RULES
ip rule add fwmark 666 lookup 666
ip route add local 0.0.0.0/0 dev lo table 666

# OUTSIDE TRAFFIC
iptables -t mangle -N dual

iptables -t mangle -A dual -d 0.0.0.0/8 -j RETURN
iptables -t mangle -A dual -d 127.0.0.0/8 -j RETURN
iptables -t mangle -A dual -d 10.0.0.0/8 -j RETURN
iptables -t mangle -A dual -d 172.16.0.0/12 -j RETURN
iptables -t mangle -A dual -d 192.168.0.0/16 -j RETURN
iptables -t mangle -A dual -d 169.254.0.0/16 -j RETURN
iptables -t mangle -A dual -d 224.0.0.0/4 -j RETURN
iptables -t mangle -A dual -d 240.0.0.0/4 -j RETURN

iptables -t mangle -A dual -p tcp -j TPROXY --on-port 1234 --tproxy-mark 666

iptables -t mangle -A PREROUTING -j dual

# INSIDE TRAFFIC
iptables -t mangle -N dual_local

iptables -t mangle -A dual_local -d 0.0.0.0/8 -j RETURN
iptables -t mangle -A dual_local -d 127.0.0.0/8 -j RETURN
iptables -t mangle -A dual_local -d 10.0.0.0/8 -j RETURN
iptables -t mangle -A dual_local -d 172.16.0.0/12 -j RETURN
iptables -t mangle -A dual_local -d 192.168.0.0/16 -j RETURN
iptables -t mangle -A dual_local -d 169.254.0.0/16 -j RETUR
iptables -t mangle -A dual_local -d 224.0.0.0/4 -j RETURN
iptables -t mangle -A dual_local -d 240.0.0.0/4 -j RETURN

iptables -t mangle -A dual_local -p tcp -j MARK --set-mark 666

iptables -t mangle -A OUTPUT -p tcp -m owner --uid-owner dual -j RETURN

iptables -t mangle -A OUTPUT -j dual_local

sysctl -w net.ipv4.conf.all.route_localnet=1
iptables -t nat -A PREROUTING -p icmp -d 198.18.0.0/16 -j DNAT --to-destination 127.0.0.1