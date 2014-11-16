#!/bin/bash

wget https://www.dropbox.com/s/8h4wj8hvdxw3iye/ubuntu-home-ti.tar.gz
tar -xzf ubuntu-home-ti.tar.gz -C ~/

export CLASSPATH=~/ti/ccsv6/ccs_base/DebugServer/packages/ti/dss/java:$CLASSPATH

