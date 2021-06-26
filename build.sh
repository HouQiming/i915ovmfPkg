#!/bin/bash
export WORKSPACE=/home/patrick/development
export PCILOC=0000:00:02.0
export PCIID=8086:9bca
export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms

cd $WORKSPACE
. edk2/edksetup.sh
if [ ! -f "$WORKSPACE/edk2/BaseTools/Source/C/bin" ]; then
    make -C edk2/BaseTools
fi
build -v -b DEBUG -p i915ovmfPkg/i915ovmf.dsc || exit
