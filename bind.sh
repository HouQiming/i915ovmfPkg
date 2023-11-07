#!/bin/bash
source ./config
     echo $PCILOC > /sys/bus/pci/devices/$PCILOC/driver/unbind
     echo $PCILOC > /sys/bus/pci/drivers/i915/bind
systemctl start display-manager.service
