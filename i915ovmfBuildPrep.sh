#!/usr/bin/env bash
version="0.1.0-alpha"
OS="UnKnown"
OS_VER="0.0"
INSTALL_DIR="./i915Install"
declare -A gpus
declare PCILOC=0000:00:02.0
PCIID=8086:9bca
checkRoot() {
  if ((EUID != 0)); then
    echo "This script must be run as root"
    exit 1
  fi
  return 0
}
displayHelp() {
  echo "i915ovmf Download Script Version $version"
  echo "Setups up your system to compile and run the i915ovmf ROM"
  echo "Usage: ./i915ovmfBuildPrep.sh [OPTIONS]"
  echo "Options: "
  echo "  -V | --version: Prints the version"
  echo "  -d | --dir: the directory to setup the workspace in. Defaults to ./i915Install"
  echo "  -h | --help: Shows this help"
  return 0
}
readArgs() {
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
      dnf install libuuid-devel iasl nasm clang lld-devel llvm-devel cmake automake git
      echo "Fedora Preparation Done"
      ;;
    Ubuntu* | ubuntu*)
      echo "Detected Ubuntu $OS_VER"
      if [[ "$OS_VER" != "20.04" ]]; then
        echo "Untested version of Ubuntu. Continuing anyway. Failure may be imminent."
      fi
      apt update
      apt install iasl nasm clang lld cmake automake build-essential uuid-dev git gcc python3-distutils python-is-python3
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
  if [[ -e $INSTALL_DIR ]]; then
    echo "Selected install directory \"$INSTALL_DIR\" already exists. Please rename it or select a different directory with the -d option"
    exit 1
  fi
  echo "Preparing Workspace"
  mkdir -p $INSTALL_DIR
  cd $INSTALL_DIR
  INSTALL_DIR="$(pwd)"

  return 0
}
downloadi915() {
  git clone https://github.com/patmagauran/i915ovmfPkg.git
  return 0
}
downloadEdk2() {
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
  cd $INSTALL_DIR
  mkdir Conf
  cp i915ovmfPkg/target.txt Conf/target.txt
  return 0
}
#Inspiration for GPU Prompting comes from https://github.com/hertg/egpu-switcher/blob/master/egpu-switcher
getGPUS() {
  gpus=()
  lines=$(lspci -mm -n -D -d 8086::0300 && lspci -mm -n -D -d 8086::0302)
  while read -r line; do
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
  return 0
}

main() {
  set -o errexit
  checkRoot
  readArgs
  getDistroAndVersion
  installRequiredSoftware
  prepWorkspace
  downloadi915
  #downloadEdk2
  setupEDK2
  getGPUS
  promptGPU
  setupi915
  return 0
}

main
