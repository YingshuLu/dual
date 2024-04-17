#!/usr/bin/env bash

set -ex

ip rule del fwmark 1 table 100 || true
ip route del local 0.0.0.0/0 dev lo table 100 || true

iptables -t nat -F
iptables -t nat -X
iptables -t mangle -F
iptables -t mangle -X dual_skip || true
iptables -t mangle -X dual_mark || true
iptables -t mangle -X dual_proxy || true
