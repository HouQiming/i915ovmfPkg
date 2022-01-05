#!/bin/bash
source ./config

export GVTMODE=i915-GVTg_V5_4
export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms

cd $WORKSPACE
. edk2/edksetup.sh
if [ ! -f "$WORKSPACE/edk2/BaseTools/Source/C/bin" ]; then
    make -C edk2/BaseTools
fi
build -v -b DEBUG -p i915ovmfPkg/i915ovmf.dsc || exit
#build -b RELEASE -p i915ovmfPkg/i915ovmf.dsc || exit
mkdir i915_simple
cd ./i915_simple
#cp ../Build/i915ovmf/RELEASE_GCC5/X64/i915ovmf.rom ./ || exit
cp ../Build/i915ovmf/DEBUG_GCC5/X64/i915ovmf.rom ./ || exit

if [ -e /sys/bus/pci/devices/$PCILOC/2aee154e-7d0d-11e8-88b8-6f45320c7162 ]
then
	true
else
	modprobe kvmgt || exit
	#sudo dd if=/sys/class/drm/card0-HDMI-A-1/edid of=/sys/class/drm/card0/gvt_edid bs=128 count=1
	echo 2aee154e-7d0d-11e8-88b8-6f45320c7162 > /sys/bus/pci/devices/$PCILOC/mdev_supported_types/$GVTMODE/create || exit
fi

# Create an UEFI disk that immediately shuts down the VM when booted
mkdir -p tmpfat
mount disk tmpfat
mkdir -p tmpfat/EFI/BOOT
umount tmpfat
rmdir tmpfat

qemu-system-x86_64 -k en-us -name uefitest,debug-threads=on -serial stdio -m 2048 -M pc -cpu host -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -machine kernel_irqchip=on -nodefaults -rtc base=localtime,driftfix=slew -no-hpet -global kvm-pit.lost_tick_policy=discard -enable-kvm -bios $WORKSPACE/OVMF_CODE.fd -display gtk,gl=on,grab-on-hover=on -full-screen -vga none -device vfio-pci,sysfsdev=/sys/bus/pci/devices/$PCILOC/2aee154e-7d0d-11e8-88b8-6f45320c7162,addr=02.0,display=on,x-igd-opregion=on,romfile=`pwd`/i915ovmf.rom -device qemu-xhci,p2=8,p3=8 -device usb-kbd -device usb-tablet -drive format=raw,file=disk
