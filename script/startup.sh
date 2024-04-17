#!/usr/bin/env bash

ip link set eth0 promisc on

cwd=`pwd`

cd $cwd

bash $cwd/net_clean.sh
bash $cwd/net_setup.sh

env LD_LIBRARY_PATH=$cwd/lib $cwd/proxy
