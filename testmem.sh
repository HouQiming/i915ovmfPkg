#!/bin/sh
sudo dd if=i915ovmf.rom of=/dev/mem oflag=seek_bytes seek=`printf "%d" 0x81005000` bs=4096 count=1
sudo dd if=/dev/mem iflag=skip_bytes skip=`printf "%d" 0x81000000` bs=4096 count=1 of=page0.bin
sudo dd if=/dev/mem iflag=skip_bytes skip=`printf "%d" 0x807f0000` bs=4096 count=1 of=page1.bin
sudo dd if=/dev/mem iflag=skip_bytes skip=`printf "%d" 0x807e9000` bs=4096 count=1 of=pagef.bin
sudo dd if=/dev/mem iflag=skip_bytes skip=`printf "%d" 0x807e8000` bs=4096 count=1 of=pagee.bin

