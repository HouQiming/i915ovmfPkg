#!/bin/bash
export WORKSPACE=/home/patrick/development
export PCILOC=0000:00:02.0
export PCIID=8086:9bca
export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms

cd $WORKSPACE
rm -rf Build