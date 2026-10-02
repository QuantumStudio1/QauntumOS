#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root="$repo_dir/build/initramfs-root"
image="$repo_dir/build/qauntumos-initramfs.cpio.gz"

rm -rf -- "$root"
mkdir -p "$root/bin" "$root/dev" "$root/proc" "$root/sys" "$root/tmp" "$root/etc"
install -m 0755 "$repo_dir/build/qauntum-init" "$root/init"
install -m 0755 "$repo_dir/build/qauntum-shell" "$root/bin/sh"
printf 'NAME=QauntumOS\nVERSION=1-boot-prototype\nID=qauntumos\n' >"$root/etc/os-release"

python3 "$repo_dir/tools/write-initramfs.py" "$root" "$image"
echo "Built $image"
