
     echo 0000:00:02.0 > /sys/bus/pci/devices/0000\:00\:02\.0/driver/unbind
     echo 0000:00:02.0 > /sys/bus/pci/drivers/i915/bind
systemctl start display-manager.service
