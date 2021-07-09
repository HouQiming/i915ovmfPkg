
systemctl stop display-manager.service
     echo 8086:9bca  > /sys/bus/pci/drivers/vfio-pci/new_id
     echo 0000:00:02.0 > /sys/bus/pci/devices/0000\:00\:02\.0/driver/unbind
     echo 0000:00:02.0 > /sys/bus/pci/drivers/vfio-pci/bind
