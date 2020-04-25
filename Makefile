i915ovmf.efi: i915ovmf.o
	x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o i915ovmf.efi i915ovmf.o

i915ovmf.o: i915ovmf.c
	x86_64-w64-mingw32-gcc -I../edk2/MdePkg/Include -I../edk2/BaseTools/Source/C/Include/X64 -ffreestanding -Wall -Wextra -c -o i915ovmf.o i915ovmf.c
