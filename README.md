# VBIOS for Intel GPU Passthrough

Disclaimer: When used in direct passthrough, this VBIOS could produce bad pixel clock that can potentially damage your monitor! Make sure your monitor has protections against that. I'm not responsible for any monitor damage.

## What is this

This is an independent Video BIOS for Intel integrated GPUs. It provides a boot display and sets up an OpRegion so that Windows guests can produce monitor output.

The OpRegion code comes from IgdAssignmentDxe and should work everywhere. The boot display works for GVT-g and can safely replace ramfb. For direct passthrough, the boot display only works with the exact combination of an HDMI monitor and an Intel Skylake processor.

## How to build
1. Make a workspace folder, for example: `i915-development`
2. Clone the EDK II, EDKII-Packages, and this repo into the folder you just created
3. In the EDK II folder, run `git submodule update --init` to download the submodules
4. In the workspace folder, make a new folder called `Conf`
5. Run `cp i915ovmfPkg/target.txt Conf/target.txt`
6. Run: 
```
ln -s ./i915ovmfPkg ./edk2/
# Create an empty FAT disk image 
dd if=/dev/zero of=disk bs=128M count=1
sudo mkfs.vfat disk
```
7. Edit i915ovmfpkg/test to update the WORKSPACE variable to your workspace you created earlier.
8. Then run `./t` to build and test. Due to GVT-g and EFI shenanigans, the testing process needs root.

**NOTE** Compilation requires the following dependencies(names are not correct):
- lib-uuid-devel
- iasl
- nasm
- git
- clang
- lld-devel
- llvm-devel




If you just want to use it for your VM, grab the rom file in Releases.

## License

I have no idea what this should be licensed in, but the code came from:
- managarm OS: https://github.com/managarm/managarm/
- IgdAssignmentDxe: non-upstreamed Intel patch to OVMF
- EDK II: https://github.com/tianocore/edk2
- The Linux kernel
