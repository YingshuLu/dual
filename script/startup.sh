#!/usr/bin/env bash

grep -qw dual /etc/passwd || echo "dual:x:0:0:::" >> /etc/passwd

cwd=`pwd`
sudo -u dual env LD_LIBRARY_PATH=$cwd/lib $cwd/proxy