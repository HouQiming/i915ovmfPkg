# VBIOS for Intel GPU Passthrough

Disclaimer: When used in direct passthrough, this VBIOS could produce bad pixel clock that can potentially damage your monitor! Make sure your monitor has protections against that. I'm not responsible for any monitor damage.

## What is this

This is an independent Video BIOS for Intel integrated GPUs. It provides a boot display and sets up an OpRegion so that Windows guests can produce monitor output.

The OpRegion code comes from IgdAssignmentDxe and should work everywhere. The boot display works for GVT-g and can safely replace ramfb. For direct passthrough, the boot display only works with the exact combination of an HDMI monitor and an Intel Skylake processor.

## How to build

1. Make a workspace folder, for example: `i915-development`
2. Clone the EDK II, EDKII-Platforms, and this repo into the folder you just created
3. In the EDK II folder, run `git submodule update --init` to download the submodules
4. In the workspace folder, make a new folder called `Conf`
5. Run `cp i915ovmfPkg/target.txt Conf/target.txt`
6. Run:

```
ln -s ./i915ovmfPkg ./edk2/
# Create an empty FAT disk image
dd if=/dev/zero of=disk bs=128M count=1
sudo mkfs.vfat disk
mkdir i915_simple
cp disk i915_simple/
```

7. Edit i915ovmfpkg/test and i915ovmfpkg/test-gvt-d to update the WORKSPACE variable to your workspace you created earlier.
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
**NOTE** Currently, using this rom causes the intel thunderbolt controller to reset. Using a thunderbolt e-gpu may cause the system to crash.

## VFIO Setup
Refer to the [Arch Linux wiki Guide for passthrough](https://wiki.archlinux.org/index.php/PCI_passthrough_via_OVMF) and [for GVT-G](https://wiki.archlinux.org/index.php/Intel_GVT-g)
1. Install QEMU & the ovmf packages as appropriate for your distributions
2. Copy the ovmf.fd file from the appropriate locati
3. Add the following kernel parameters: `intel_iommu=on iommu=pt`
4. Reboot and run the included `iommu.sh` script to list your iommu groups and PCI ids. Script courtesy of Arch Linux Guide
5. In the ouput, search for the intel GPU. Note both the PCI location(eg 00:02.0) and the id(eg 8086:1234)
6. Follow the appropriate instructions below for what you are doing (GVT-d/g)

### GVT-G
1. Enable the kernel modules: `kvmgt`,`vfio-iommu-type1`,`vfio-mdev`
2. Add the following kernel parameter: `i915.enable_gvt=1`
3. Reboot and determine your avaiable GVT-g modes by running the folloiwng command: `ls /sys/devices/pci${GVT_DOM}/$GVT_PCI/mdev_supported_types` where GVT_DOM is the PCI Domain(Typically 0000) and GVT_PCI is the PCI location noted earlier. If you have trouble, use the command lspci -D -nn and locate the intel gpu to get the correct domain/location.
4. Update the Type variable in test to one of the listed modes for your gpu.

### GVT-d
1. Add the following kernel parameter: `vfio-pci.ids=PCIID` where PCIID is the ID obtained earlier(eg 8086:1234)
2. Enable the following kernel modules: `vfio_pci vfio vfio_iommu_type1 vfio_virqfd`
3. Reboot
4. Update the test-gvt-d with the correct PCI location
## License

I have no idea what this should be licensed in, but the code came from:

- managarm OS: https://github.com/managarm/managarm/
- IgdAssignmentDxe: non-upstreamed Intel patch to OVMF
- EDK II: https://github.com/tianocore/edk2
- The Linux kernel
