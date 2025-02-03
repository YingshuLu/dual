#!/usr/bin/env bash

ip link set eth0 promisc on

cwd=`dirname "$0"`

port=`cat dual.json | jq '.listen_port'`

cd $cwd

bash $cwd/net_clean.sh
bash $cwd/net_setup.sh $port

env LD_LIBRARY_PATH=$cwd/lib $cwd/proxy
