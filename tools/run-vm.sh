#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kernel="$repo_dir/build/kernel-out/arch/x86/boot/bzImage"
initramfs="$repo_dir/build/qauntumos-initramfs.cpio.gz"
qemu=${QAUNTUM_QEMU:-qemu-system-x86_64}
display=${QAUNTUM_DISPLAY:-none}
share_args=()
kernel_args='console=tty0 console=ttyS0 rdinit=/init loglevel=4 vga=791'
if [[ ${QAUNTUM_RECOVERY:-0} == 1 ]]; then
    kernel_args+=' qauntum.recovery=1'
fi
disk_args=()
if [[ -n ${QAUNTUM_DATA_IMAGE:-} ]]; then
    disk_args=(-drive "file=$QAUNTUM_DATA_IMAGE,format=raw,if=virtio")
    kernel_args+=' qauntum.data=/dev/vda'
fi
if [[ -n ${QAUNTUM_QEMU_SHARE:-} ]]; then
    share_args=(-L "$QAUNTUM_QEMU_SHARE")
fi

exec "$qemu" "${share_args[@]}" \
    -m 1024 -smp 2 -display "$display" -monitor none -serial stdio \
    "${disk_args[@]}" \
    -kernel "$kernel" -initrd "$initramfs" \
    -append "$kernel_args" "$@"
