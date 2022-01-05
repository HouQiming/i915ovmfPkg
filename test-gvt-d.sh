#!/bin/bash
source ./config

export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms

cd $WORKSPACE
. edk2/edksetup.sh
if [ ! -f "$WORKSPACE/edk2/BaseTools/Source/C/bin" ]; then
    make -C edk2/BaseTools
fi
build -v -b DEBUG -p i915ovmfPkg/i915ovmf.dsc || exit
#build -b RELEASE -p i915ovmfPkg/i915ovmf.dsc || exit
mkdir -p i915_simple
cd ./i915_simple
#cp ../Build/i915ovmf/RELEASE_GCC5/X64/i915ovmf.rom ./ || exit
cp ../Build/i915ovmf/DEBUG_GCC5/X64/i915ovmf.rom ./ || exit

# Create an UEFI disk that immediately shuts down the VM when booted
mkdir -p tmpfat
mount disk tmpfat
mkdir -p tmpfat/EFI/BOOT
umount tmpfat
rmdir tmpfat
systemctl stop display-manager.service
     echo $PCIID > /sys/bus/pci/drivers/vfio-pci/new_id
     echo $PCILOC> /sys/bus/pci/devices/$PCILOC/driver/unbind
     echo $PCILOC > /sys/bus/pci/drivers/vfio-pci/bind
     echo 0 > /sys/class/vtconsole/vtcon0/bind
     echo 0 > /sys/class/vtconsole/vtcon1/bind
     echo efi-framebuffer.0 > /sys/bus/platform/drivers/efi-framebuffer/unbind
#qemu-system-x86_64 -k en-us -name uefitest,debug-threads=on -nographic -vga none -serial stdio -m 2048 -M pc -cpu host -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -machine kernel_irqchip=on -nodefaults -rtc base=localtime,driftfix=slew -no-hpet -global kvm-pit.lost_tick_policy=discard -enable-kvm -bios $WORKSPACE/OVMF_CODE.fd -device vfio-pci,host=$PCILOC,romfile=`pwd`/i915ovmf.rom -device qemu-xhci,p2=8,p3=8 -device usb-kbd -device usb-tablet -drive format=raw,file=disk -usb  
timeout --foreground -k 1 8 qemu-system-x86_64 -k en-us -name uefitest,debug-threads=on -nographic -vga none -chardev stdio,id=char0,logfile=serial.log,signal=off \
  -serial chardev:char0 -m 2048 -M pc -cpu host -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -machine kernel_irqchip=on -nodefaults -rtc base=localtime,driftfix=slew -no-hpet -global kvm-pit.lost_tick_policy=discard -enable-kvm -bios $WORKSPACE/OVMF_CODE.fd -device vfio-pci,host=$PCILOC,romfile=`pwd`/i915ovmf.rom -device qemu-xhci,p2=8,p3=8 -device usb-kbd -device usb-tablet -drive format=raw,file=disk -usb  
