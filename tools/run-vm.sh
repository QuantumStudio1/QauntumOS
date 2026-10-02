#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
kernel="$repo_dir/build/kernel-out/arch/x86/boot/bzImage"
initramfs="$repo_dir/build/qauntumos-initramfs.cpio.gz"
qemu=${QAUNTUM_QEMU:-qemu-system-x86_64}
share_args=()
if [[ -n ${QAUNTUM_QEMU_SHARE:-} ]]; then
    share_args=(-L "$QAUNTUM_QEMU_SHARE")
fi

exec "$qemu" "${share_args[@]}" \
    -m 1024 -smp 2 -display none -monitor none -serial stdio \
    -kernel "$kernel" -initrd "$initramfs" \
    -append 'console=ttyS0 rdinit=/init loglevel=4' "$@"
