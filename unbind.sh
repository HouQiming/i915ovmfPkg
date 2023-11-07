#!/bin/bash
source ./config
systemctl stop display-manager.service
     echo $PCIID > /sys/bus/pci/drivers/vfio-pci/new_id
     echo $PCILOC> /sys/bus/pci/devices/$PCILOC/driver/unbind
     echo $PCILOC > /sys/bus/pci/drivers/vfio-pci/bind
     echo 0 > /sys/class/vtconsole/vtcon0/bind
     echo 0 > /sys/class/vtconsole/vtcon1/bind
     echo efi-framebuffer.0 > /sys/bus/platform/drivers/efi-framebuffer/unbind
