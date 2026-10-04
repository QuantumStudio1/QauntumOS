#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(<"$repo_dir/config/kernel.version")
archive="$repo_dir/build/downloads/linux-${version}.tar.xz"
source_dir="$repo_dir/build/linux-${version}"
output_dir="$repo_dir/build/kernel-out"

bash "$repo_dir/tools/fetch-kernel.sh"
if [[ ! -d $source_dir ]]; then
    tar -C "$repo_dir/build" -xf "$archive"
fi
mkdir -p "$output_dir"
if [[ ! -f $output_dir/.config ]]; then
    make -C "$source_dir" O="$output_dir" x86_64_defconfig
fi
"$source_dir/scripts/config" --file "$output_dir/.config" \
    -e BLK_DEV_INITRD -e DEVTMPFS -e DEVTMPFS_MOUNT \
    -e SERIAL_8250 -e SERIAL_8250_CONSOLE \
    -e FB -e FB_VESA -e FRAMEBUFFER_CONSOLE \
    -e SYSFB_SIMPLEFB -e DRM_SIMPLEDRM -e DRM_FBDEV_EMULATION \
    -e JOYSTICK_XPAD -e HID_SONY -e HID_PLAYSTATION \
    -d DEBUG_INFO_BTF -d SYSTEM_TRUSTED_KEYS -d SYSTEM_REVOCATION_KEYS
make -C "$source_dir" O="$output_dir" olddefconfig
jobs=${QAUNTUM_JOBS:-4}
make -C "$source_dir" O="$output_dir" -j"$jobs" bzImage
echo "Built $output_dir/arch/x86/boot/bzImage"
