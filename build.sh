#!/bin/bash
source ./config

export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms

cd $WORKSPACE
. edk2/edksetup.sh
if [ ! -f "$WORKSPACE/edk2/BaseTools/Source/C/bin" ]; then
    make -C edk2/BaseTools
fi
build -v -b DEBUG -p i915ovmfPkg/i915ovmf.dsc || exit
