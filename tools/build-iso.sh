#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$repo_dir/build"
stage="$build_dir/iso-root"
host="$build_dir/host-tools/usr"
kernel="$build_dir/kernel-out/arch/x86/boot/bzImage"
initramfs="$build_dir/qauntumos-initramfs.cpio.gz"
iso="$build_dir/QauntumOS-Version-2-dev-x86_64.iso"

for file in "$kernel" "$initramfs"; do
    [[ -s $file ]] || { echo "Missing $file; run make kernel initramfs" >&2; exit 1; }
done

syslinux_dir="$host/lib/syslinux/bios"
[[ -d $syslinux_dir ]] || syslinux_dir=/usr/lib/syslinux/bios
grub_dir="$host/lib/grub/x86_64-efi"
[[ -d $grub_dir ]] || grub_dir=/usr/lib/grub/x86_64-efi
grub_tool="$host/bin/grub-mkstandalone"
[[ -x $grub_tool ]] || grub_tool=$(command -v grub-mkstandalone || true)
isolinux_bin=${QAUNTUM_ISOLINUX_BIN:-$syslinux_dir/isolinux.bin}
ldlinux=${QAUNTUM_LDLINUX_C32:-$syslinux_dir/ldlinux.c32}
isohdpfx=${QAUNTUM_ISOHDPFX_BIN:-$syslinux_dir/isohdpfx.bin}
grub_mkstandalone=${QAUNTUM_GRUB_MKSTANDALONE:-$grub_tool}
grub_modules=${QAUNTUM_GRUB_MODULES:-$grub_dir}
for file in "$isolinux_bin" "$ldlinux" "$isohdpfx" "$grub_mkstandalone"; do
    [[ -f $file ]] || { echo "Missing ISO boot tool $file (install Syslinux and GRUB)" >&2; exit 1; }
done
[[ -d $grub_modules ]] || { echo "Missing GRUB modules $grub_modules" >&2; exit 1; }
for command in xorriso mkfs.fat mcopy mmd truncate; do
    command -v "$command" >/dev/null || { echo "Missing $command" >&2; exit 1; }
done

rm -rf "$stage"
mkdir -p "$stage/boot" "$stage/isolinux" "$stage/EFI"
cp "$kernel" "$stage/boot/vmlinuz"
cp "$initramfs" "$stage/boot/initramfs.cpio.gz"
cp "$isolinux_bin" "$ldlinux" "$stage/isolinux/"

cat > "$stage/isolinux/isolinux.cfg" <<'EOF'
PROMPT 0
TIMEOUT 30
DEFAULT qauntumos
LABEL qauntumos
  LINUX /boot/vmlinuz
  INITRD /boot/initramfs.cpio.gz
  APPEND rdinit=/init console=tty0 console=ttyS0 loglevel=4 vga=791
LABEL recovery
  LINUX /boot/vmlinuz
  INITRD /boot/initramfs.cpio.gz
  APPEND rdinit=/init console=tty0 console=ttyS0 loglevel=4 vga=791 qauntum.recovery=1
EOF

cat > "$build_dir/grub-iso.cfg" <<'EOF'
set timeout=3
set default=0
search --no-floppy --set=root --label QAUNTUMOS_V2
menuentry "QauntumOS Account Preview" {
    linux /boot/vmlinuz rdinit=/init console=tty0 console=ttyS0 loglevel=4
    initrd /boot/initramfs.cpio.gz
}
menuentry "QauntumOS Recovery" {
    linux /boot/vmlinuz rdinit=/init console=tty0 console=ttyS0 loglevel=4 qauntum.recovery=1
    initrd /boot/initramfs.cpio.gz
}
EOF

"$grub_mkstandalone" -O x86_64-efi -d "$grub_modules" \
    -o "$build_dir/BOOTX64.EFI" \
    "boot/grub/grub.cfg=$build_dir/grub-iso.cfg"
mkdir -p "$stage/EFI/BOOT"
cp "$build_dir/BOOTX64.EFI" "$stage/EFI/BOOT/BOOTX64.EFI"

efi_image="$stage/EFI/efiboot.img"
truncate -s 16M "$efi_image"
mkfs.fat -F 16 -n QAUNTUMEFI "$efi_image" >/dev/null
mmd -i "$efi_image" ::/EFI ::/EFI/BOOT
mcopy -i "$efi_image" "$build_dir/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI

xorriso -as mkisofs -r -J -V QAUNTUMOS_V2 -o "$iso" \
    -b isolinux/isolinux.bin -c isolinux/boot.cat \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    -eltorito-alt-boot -e EFI/efiboot.img -no-emul-boot \
    -isohybrid-mbr "$isohdpfx" -isohybrid-gpt-basdat "$stage" >/dev/null
sha256sum "$iso" > "$iso.sha256"
echo "Built $iso"
