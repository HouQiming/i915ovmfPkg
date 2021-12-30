## Repository Activity Notice
As you may have noticed, I am not very active in development of this anymore. While I do not plan to fully step away from this, I will be less likely to respond to issues quickly. I welcome the community to come together to work on this. I am happy to provide support to anyone who wants to expand this codebase to more generations of intel GPUs or fix any of the various bugs. 

## Discord
Since I will not be as active, I have created a discord server that I encourage everyone to join to better recieve support from the community (and myself when I can).

<a href="https://discord.gg/pZyzrfCYrJ">
<img src="https://img.shields.io/discord/925800342598340660?logo=discord&label=Discord&style=for-the-badge&color=228B22"
 alt="chat on Discord"></a>

# VBIOS for Intel GPU Passthrough

This project attempts to create a UEFI driver for the intel integrated GPUS so that they can be used in VFIO Passthrough. Prior to this driver, there was no easy or reliable solution to both virtualized and direct passthrough(GVT-G/D). This driver adds an opRegion for the iGPU to utilize during the Boot Process, allowing for access to the UEFI menus and any other interfaces that are created before an operating system level driver is initialized. As a bonus, this allows for MacOS to boot in this virtual environment.

## Notice

*This Software deals directly with the graphics hardware and interfaces. I assume no responsibility should it cause any damage to your GPUs, Cables, Displays, Other hardware, or persons. It has been tested on my personal machine, but **you are using this software at your own risk.***

Disclaimer: When used in direct passthrough, this VBIOS could produce bad pixel clock that can potentially damage your monitor! Make sure your monitor has protections against that. I'm not responsible for any monitor damage.

## Current Feature Support

* Boot a virtual intel GPU(GVT-G)
* Passthrough the entire Intel GPU (GVT-D)
  * Display Port interfaces
  * eDP interfaces, including Laptop Screens
  * HDMI interfaces
* Theoretically compatible with any 14nm chip(Skylake, Kaby lake, Coffee Lake, Amber Lake, Whiskey Lake, Comet Lake).
* Auto detect Outputs and types(**NEW**)

## Possible Features to come

* Allow for generation Specific Quirks
* Allow for other it to work with other intel CPU Generations.

## Known Issues

* May have issues with thunderbolt eGPUs. If you encounter problems, try with it unplugged
* GVT-G may struggle with external displays(even if through an eGPU or other GPU)
* May cause random kernel panics with MacOS due to a low default DVMT Pre-allocated memory amount. See [here](https://github.com/patmagauran/i915ovmfPkg/wiki/DVMT-Pre-Alloc---Stolen-Memory-Issues) for more info

## What is this

This is an independent Video BIOS for Intel integrated GPUs. It provides a boot display and sets up an OpRegion so that Windows guests can produce monitor output.

The OpRegion code comes from IgdAssignmentDxe and should work everywhere. The boot display works for GVT-g and can safely replace ramfb. For direct passthrough, the boot display Works on intel 14nm based CPUS and HDMI/DP/eDP displays(Inlcuding laptop Screens!).

## Usage

Please see the Wiki for more information regarding compiling, usage, or further information.

## License

I have no idea what this should be licensed in, but the code came from:

- managarm OS: https://github.com/managarm/managarm/
- IgdAssignmentDxe: non-upstreamed Intel patch to OVMF
- EDK II: https://github.com/tianocore/edk2
- The Linux kernel
- Intel-gpu-tools: https://cgit.freedesktop.org/xorg/app/intel-gpu-tools/tree/tools
