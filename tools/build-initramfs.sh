#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root="$repo_dir/build/initramfs-root"
image="$repo_dir/build/qauntumos-initramfs.cpio.gz"

rm -rf -- "$root"
mkdir -p "$root/bin" "$root/dev" "$root/proc" "$root/sys" "$root/tmp" "$root/etc" \
    "$root/var/lib/qauntumos/accounts"
install -m 0755 "$repo_dir/build/qauntum-init" "$root/init"
install -m 0755 "$repo_dir/build/qauntum-shell" "$root/bin/sh"
install -m 0755 "$repo_dir/build/qauntum-session" "$root/bin/qauntum-session"
chmod 0700 "$root/var/lib/qauntumos/accounts"
while IFS= read -r library; do
    cp -L --parents -- "$library" "$root"
done < <(ldd "$repo_dir/build/qauntum-session" | \
    awk '{for (i=1; i<=NF; i++) if ($i ~ /^\//) {sub(/\(.*/, "", $i); print $i}}' | sort -u)
printf 'NAME=QauntumOS\nVERSION=2-account-preview\nID=qauntumos\n' >"$root/etc/os-release"

python3 "$repo_dir/tools/write-initramfs.py" "$root" "$image"
echo "Built $image"
