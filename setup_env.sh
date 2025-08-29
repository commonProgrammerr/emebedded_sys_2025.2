#! /usr/bin/env bash


if [[ "$EUID" -ne 0 ]]; then
  echo "This script must run with root privileges." 1>&2
  exit 1
fi

apt update
apt install -y gcc-arm-none-eabi binutils-arm-none-eabi # arm-none-eabi-newlib
apt install -y make cmake build-essential
apt install -y stlink-tools
