#! /bin/bash
version="0.1.0-alpha"
OS="UnKnown"
OS_VER="0.0"
INSTALL_DIR="./i915Install"
declare -A gpus
declare PCILOC=0000:00:02.0
PCIID=8086:9bca
COMMAND="setup"
InstallLibVirtStuff=1
modulesLoadFile=/etc/modules-load.d/i915ovmf.conf
checkRoot() {
  if ((EUID != 0)); then
    echo "This script must be run as root"
    exit 1
  fi
  return 0
}
displayHelp() {
  echo "i915ovmf Download Script Version $version"
  echo "Manages everything related to development, building, and testing of the i915ovmf ROM"
  echo "Usage: ./i915ovmfBuildPrep.sh [setup | build | clean | update | kernel | GVT-G | GVT-D] [OPTIONS]"
  echo "Options: "
  echo "  -V | --version: Prints the version"
  echo "  -d | --dir: the directory to setup the workspace in. Defaults to ./i915Install"
  echo "  -h | --help: Shows this help"
  return 0
}
readArgs() {

  COMMAND=$1
  echo "Running in $COMMAND mode"
  shift
  #Parse arguments; Taken from https://devhints.io/bash#miscellaneous
  while [[ "$1" =~ ^- && ! "$1" == "--" ]]; do
    case $1 in
      -V | --version)
        echo $version
        exit
        ;;
      -d | --dir)
        shift
        INSTALL_DIR=$1
        ;;
      -h | --help)
        displayHelp
        exit
        ;;
      *)
        echo "Invalid Option: $1 specified"
        ;;
    esac
    shift
  done
  if [[ "$1" == '--' ]]; then shift; fi
  return 0
}

getDistroAndVersion() {
  if [ -f /etc/os-release ]; then
    # freedesktop.org and systemd
    . /etc/os-release
    OS=$NAME
    OS_VER=$VERSION_ID
  elif type lsb_release >/dev/null 2>&1; then
    # linuxbase.org
    OS=$(lsb_release -si)
    OS_VER=$(lsb_release -sr)
  elif [ -f /etc/lsb-release ]; then
    # For some versions of Debian/Ubuntu without lsb_release command
    . /etc/lsb-release
    OS=$DISTRIB_ID
    OS_VER=$DISTRIB_RELEASE
  elif [ -f /etc/debian_version ]; then
    # Older Debian/Ubuntu/etc.
    OS=Debian
    OS_VER=$(cat /etc/debian_version)
  else
    # Fall back to uname, e.g. "Linux <version>", also works for BSD, etc.
    OS=$(uname -s)
    OS_VER=$(uname -r)
  fi
  return 0
}
installRequiredSoftware() {
  echo "Installing pre-requisite software"

  case $OS in
    Fedora* | fedora*)
      echo "Detected Fedora $OS_VER"
      if [[ "$OS_VER" != "32" ]]; then
        echo "Untested version of Fedora. Continuing anyway. Failure may be imminent."
      fi
      dnf update
      installSoftware="libuuid-devel iasl nasm clang lld-devel llvm-devel cmake automake git"
      if (($InstallLibVirtStuff)); then
        installSoftware+=" libvirt virt-install qemu-kvm bridge-utils libguestfs-tools virt-manager"
      fi
      dnf install $installSoftware
      echo "Fedora Preparation Done"
      ;;
    Ubuntu* | ubuntu*)
      echo "Detected Ubuntu $OS_VER"
      if [[ "$OS_VER" != "20.04" ]]; then
        echo "Untested version of Ubuntu. Continuing anyway. Failure may be imminent."
      fi
      installSoftware=" iasl nasm clang lld cmake automake build-essential uuid-dev git gcc python3-distutils python-is-python3"
      if (($InstallLibVirtStuff)); then
        installSoftware+=" libvirt-daemon qemu-kvm bridge-utils libguestfs-tools virt-manager"
      fi
      apt update
      apt install $installSoftware
      echo "Ubuntu Preparation Done"
      ;;
    *)
      echo "Unkown OS: $OS $OS_VER"
      echo "Please follow manual instructions here: https://github.com/patmagauran/i915ovmfPkg/wiki/Compiling"
      exit 1
      ;;
  esac
  return 0
}

prepWorkspace() {
  echo "$COMMAND"
  if [[ -f $INSTALL_DIR ]]; then
    echo "You selected a file as the installation directory. This is not supported. Please select a different directory with the -d option."
    exit 1
  elif [[ -d $INSTALL_DIR ]] && [[ ! -z "$(ls -A $INSTALL_DIR)" ]] && [[ $COMMAND == "setup" ]]; then
    echo "Selected install directory \"$INSTALL_DIR\" is not empty. Please rename it or select a different directory with the -d option"
    exit 1
  fi
  echo "Preparing Workspace"
  mkdir -p $INSTALL_DIR
  cd $INSTALL_DIR
  INSTALL_DIR="$(pwd)"
  echo "Installing to $INSTALL_DIR"
  return 0
}
downloadi915() {
  echo "Downloading i915ovmfpkg main repo..."
  git clone https://github.com/patmagauran/i915ovmfPkg.git
  return 0
}
downloadEdk2() {
  echo "Downloading edk2 repo..."

  git clone https://github.com/tianocore/edk2.git
  git clone https://github.com/tianocore/edk2-platforms.git
  cd edk2-platforms/
  git submodule update --init
  cd ../edk2
  git switch stable/202011
  git submodule update --init
  cd ..
  return 0
}
setupEDK2() {
  echo "Copying Config file..."

  cd $INSTALL_DIR
  mkdir Conf
  cp i915ovmfPkg/target.txt Conf/target.txt
  return 0
}
#Inspiration for GPU Prompting comes from https://github.com/hertg/egpu-switcher/blob/master/egpu-switcher
getGPUS() {
  gpus=()
  lines=$(lspci -mm -n -D -d 8086::0300 && lspci -mm -n -D -d 8086::0302 && lspci -mm -n -D -d 8086::0380)

  while read -r line; do
    echo "DEBUG: GPU FOUND: $line"
    lineElements=($line)
    bus=${lineElements[0]}
    vendor=${lineElements[2]}
    temp="${vendor%\"}"
    temp="${temp#\"}"
    vendor=$temp
    device=${lineElements[3]}
    temp="${device%\"}"
    temp="${temp#\"}"
    device=$temp
    pciID="${vendor}:${device}"
    echo "DEBUG: GPU DECODED AS $bus  $pciID"
    gpus+=([$bus]=$pciID)
  done <<<$lines

  return 0
}
promptGPU() {
  num_of_gpus=${#gpus[@]}
  if (($num_of_gpus > 1)); then
    gpu=0
    declare mapping=()
    declare i=0
    echo "More than 2 Intel GPUs detected. Please select the correct one"
    for key in ${!gpus[@]}; do
      i=$((i + 1))
      mapping+=([${i}]=${key})
      echo "  $i: $(lspci -s ${key}) (${key})"
    done
    echo "Please select which GPU the software should configure: "
    read gpu
    if ((gpu < 0 || gpu > $num_of_gpus)); then
      echo "Invalid input. Please try again"
      promptGPU
      return 0
    fi
    PCILOC=${mapping[$gpu]}
    PCIID=${gpus[${PCILOC}]}
  elif (($num_of_gpus == 1)); then
    keys=${!gpus[@]}
    PCILOC=${keys[0]}
    echo "Using Intel GPU: $(lspci -s ${PCILOC})"
    PCIID=${gpus[${PCILOC}]}
  else
    echo "No Intel GPUS detected. You can try a manual install if needed."
    exit 1
  fi
  return 0
}
setupi915() {
  cd $INSTALL_DIR/i915ovmfPkg
  if ([[ -e config ]]); then
    mv config config.bak.$(date +'%F_%T')
  fi
  echo "export PCIID=${PCIID}" >>config
  echo "export PCILOC=${PCILOC}" >>config
  echo "export WORKSPACE=${INSTALL_DIR}" >>config
  echo "export DefaultGVTMODE=1" >>config
  cd $INSTALL_DIR
  dd if=/dev/zero of=disk bs=128M count=1
  sudo mkfs.vfat disk
  mkdir i915_simple
  cp disk i915_simple/
  case $OS in
    Fedora* | fedora*)
      cp /usr/share/edk2/ovmf/OVMF_CODE.fd ./
      ;;
    Ubuntu* | ubuntu*)
      cp /usr/share/OVMF/OVMF_CODE.fd ./
      ;;
  esac
  return 0
}
buildi915() {
  echo "Building edk2 and i915 with gcc $(gcc --version)"
  cd $WORKSPACE
  . edk2/edksetup.sh
  if [ ! -f "$WORKSPACE/edk2/BaseTools/Source/C/bin" ]; then
    make -C edk2/BaseTools
  fi
  build -v -b DEBUG -p i915ovmfPkg/i915ovmf.dsc || exit
  return 0
}
clean() {
  cd $WORKSPACE
  rm -rf Build
  return 0
}
checkKernelParam() {

  return 0
}
buildKernelParams() {
  currentParams=$(sed 's/GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"/\1/p' -n /etc/default/grub)
  desiredParams=($1)
  finalParams=$currentParams
  for param in "${desiredParams[@]}"; do
    if [[ ! $currentParams =~ $param ]]; then
      finalParams="${finalParams} ${param}"
    fi
  done
  echo $finalParams
  return 0
}
updateGrub() {
  case $OS in
    Fedora* | fedora*)
      grub2-mkconfig
      ;;
    Ubuntu* | ubuntu*)
      update-grub
      ;;
  esac

}
kernel() {
  #'s/GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"/\1/p' -n
  # 's/GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"/GRUB_CMDLINE_LINUX_DEFAULT="\1 intel_iommu=on i915.enable_gvt=1 i915.enable_guc=0 iommu=pt"/g'
  saveChange=N
  backup=/etc/default/grub.bak.$(date +'%F_%T')
  cp /etc/default/grub $backup
  params="$(buildKernelParams 'intel_iommu=on i915.enable_gvt=1 i915.enable_guc=0 iommu=pt vfio-pci.ids='${PCIID}'')"
  sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"/GRUB_CMDLINE_LINUX_DEFAULT="'"${params}"'"/g' /etc/default/grub
  echo "/etc/default/grub now reads as:"
  cat /etc/default/grub
  echo
  read -p "Does this look good [Y/N]?" saveChange
  case $saveChange in
    Y | y)
      updateGrub
      ;;
    *)
      echo "Reverting kernel changes."
      cp $backup /etc/default/grub
      ;;
  esac

  touch $modulesLoadFile
  echo kvmgt >>$modulesLoadFile
  echo vfio-iommu-type1 >>$modulesLoadFile
  echo vfio-mdev >>$modulesLoadFile
  echo vfio_pci >>$modulesLoadFile

  echo "You will need to reboot for changes to take effect. Would you like to reboot now? [Y/N]"
  read saveChange
  if [[ $saveChange == "Y" || $saveChange == "y" ]]; then
    shutdown -r
  fi
  return 0
}
update() {
  cd $INSTALL_DIR
  cd i915ovmfPkg
  git pull
  echo "Updates pulled. You must now build. Run ./i915ovmfBuildPrep.sh build"
  return 0
}

promptGvtMode() {
  mode=$DefaultGVTMODE
  declare mapping=()
  declare i=0
  directory="/sys/bus/pci/devices/$PCILOC/mdev_supported_types"
  for key in "$directory"/*; do
    key=${key##*/}
    #echo $key
    i=$((i + 1))
    mapping+=([${i}]=${key})
    echo "  $i: (${key})"
    cat /sys/bus/pci/devices/$PCILOC/mdev_supported_types/$key/description | sed 's/^/      /'
  done
  echo "Please select the GVT Mode to use. Will continue with default in 10 seconds: [${DefaultGVTMODE}] "
  read -t 10 mode
  if ((mode < 1 || mode > $i)); then
    echo "Invalid input. Exiting"
    exit 1
  fi
  GVTMODE=${mapping[$mode]}
  return 0
}
gvt-g() {
  cd $WORKSPACE
  #buildi915
  #build -b RELEASE -p i915ovmfPkg/i915ovmf.dsc || exit
  mkdir -p i915_simple
  cd ./i915_simple
  #cp ../Build/i915ovmf/RELEASE_GCC5/X64/i915ovmf.rom ./ || exit
  cp ../Build/i915ovmf/DEBUG_GCC5/X64/i915ovmf.rom ./ || exit

  if [ -e /sys/bus/pci/devices/$PCILOC/2aee154e-7d0d-11e8-88b8-6f45320c7162 ]; then
    true
  else
    modprobe kvmgt || exit
    promptGvtMode
    #sudo dd if=/sys/class/drm/card0-HDMI-A-1/edid of=/sys/class/drm/card0/gvt_edid bs=128 count=1
    echo 2aee154e-7d0d-11e8-88b8-6f45320c7162 >/sys/bus/pci/devices/$PCILOC/mdev_supported_types/$GVTMODE/create || exit
  fi

  # Create an UEFI disk that immediately shuts down the VM when booted
  mkdir -p tmpfat
  mount disk tmpfat
  mkdir -p tmpfat/EFI/BOOT
  umount tmpfat
  rmdir tmpfat

  qemu-system-x86_64 -k en-us -name uefitest,debug-threads=on -serial stdio -m 2048 -M pc -cpu host -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -machine kernel_irqchip=on -nodefaults -rtc base=localtime,driftfix=slew -no-hpet -global kvm-pit.lost_tick_policy=discard -enable-kvm -bios $WORKSPACE/OVMF_CODE.fd -display gtk,gl=on,grab-on-hover=on -full-screen -vga none -device vfio-pci,sysfsdev=/sys/bus/pci/devices/$PCILOC/2aee154e-7d0d-11e8-88b8-6f45320c7162,addr=02.0,display=on,x-igd-opregion=on,romfile=$(pwd)/i915ovmf.rom -device qemu-xhci,p2=8,p3=8 -device usb-kbd -device usb-tablet -drive format=raw,file=disk

  return 0
}
rebindi915() {
  echo $PCILOC >/sys/bus/pci/devices/$PCILOC/driver/unbind || echo "Error unbinding igpu. Continuing."
  echo "i915" >/sys/bus/pci/devices/$PCILOC/driver_override || echo "Error adding igpu to i915. Continuing."
  echo 1 >/sys/bus/pci/devices/$PCILOC/reset || echo "Error resetting igpu. Continuing."
  systemctl restart display-manager.service
  return 0
}
gvt-d() {
  cd $WORKSPACE
  buildi915
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
  #echo $PCIID >/sys/bus/pci/drivers/vfio-pci/new_id

  echo $PCILOC >/sys/bus/pci/devices/$PCILOC/driver/unbind || echo "Error unbinding igpu. Continuing."
  echo "vfio-pci" >/sys/bus/pci/devices/$PCILOC/driver_override || echo "Error adding igpu to vfio. Continuing."
  echo $PCILOC >/sys/bus/pci/drivers/vfio-pci/bind || echo "Error binding igpu to vfio. Continuing."
  #qemu-system-x86_64 -k en-us -name uefitest,debug-threads=on -nographic -vga none -serial stdio -m 2048 -M pc -cpu host -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -machine kernel_irqchip=on -nodefaults -rtc base=localtime,driftfix=slew -no-hpet -global kvm-pit.lost_tick_policy=discard -enable-kvm -bios $WORKSPACE/OVMF_CODE.fd -device vfio-pci,host=$PCILOC,romfile=`pwd`/i915ovmf.rom -device qemu-xhci,p2=8,p3=8 -device usb-kbd -device usb-tablet -drive format=raw,file=disk -usb
  timeout --foreground -k 1 4 qemu-system-x86_64 -k en-us -name uefitest,debug-threads=on -nographic -vga none -chardev stdio,id=char0,logfile=serial.log,signal=off \
    -serial chardev:char0 -m 2048 -M pc -cpu host -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 -machine kernel_irqchip=on -nodefaults -rtc base=localtime,driftfix=slew -no-hpet -global kvm-pit.lost_tick_policy=discard -enable-kvm -bios $WORKSPACE/OVMF_CODE.fd -device vfio-pci,host=$PCILOC,romfile=$(pwd)/i915ovmf.rom -device qemu-xhci,p2=8,p3=8 -device usb-kbd -device usb-tablet -drive format=raw,file=disk -usb
  rebindi915
  return 0
}

promptKernel() {
  resp=Y
  read -p "Would you like to automatically configure your kernel settings for GVT-D/G? [Y/N]" resp
  case $resp in
    Y | y)
      kernel
      ;;
  esac
}
checkConfig() {
    if [[ ! -e $INSTALL_DIR/i915ovmfPkg/config ]]; then
      echo "Could not find config file. Ensure the correct workspace directory is set with the -d option. Or if this is the first run, run the program in setup mode. i.e sudo $0 setup -d EXAMPLEINSTALLDIR"
      exit 1
    fi
    source $INSTALL_DIR/i915ovmfPkg/config
    export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms
    return 0
}
main() {
  set -o errexit
  if [[ $# -eq 0 ]]; then
    displayHelp
    exit 0
  fi
  checkRoot
  readArgs $@
  getDistroAndVersion
  prepWorkspace
  case $COMMAND in
    setup)
      echo "Running Setup"
      installRequiredSoftware
      downloadi915
      downloadEdk2
      setupEDK2
      getGPUS
      promptGPU
      setupi915
      promptKernel
      ;;
    build)
      echo "Running build"
      checkConfig
      buildi915
      ;;
    clean)
      echo "Running clean"
      checkConfig
      clean
      ;;
    kernel)
      echo "Running kernel Setup"
      checkConfig
      kernel
      ;;
    update)
      echo "Running update"
      checkConfig
      update
      ;;
    GVT-G)
      echo "Running GVT-G Test"
      checkConfig
      gvt-g
      ;;
    GVT-D)
      echo "Running GVT-D Test"
      checkConfig
      gvt-d
      ;;
    bind)
      echo "rebinding igpu to i915"
      checkConfig
      rebindi915
      ;;
    *)
      echo "Invalid command entered. Please consult documentation"
      checkConfig
      displayHelp
      exit
      ;;
  esac
  return 0
}

main $@
