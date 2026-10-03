# Build and boot the Version 1 live ISO prototype

The ISO boots through BIOS and UEFI firmware. It runs entirely in memory and does not install to disk. The direct QEMU boot path remains available for quick development tests.

## Host requirements

- x86_64 Linux host with GCC, static glibc, make, Python 3, curl, xz, and GnuPG.
- Kernel build tools: bc, flex, bison, OpenSSL development headers, ELF development headers, and a C toolchain.
- QEMU's `qemu-system-x86_64` for VM booting.
- For ISO creation: Syslinux BIOS boot files, GRUB with x86_64 EFI modules, xorriso, dosfstools, and mtools.
- Several GB of free disk space for the kernel source and build output.

## Commands

From the repository root:

```sh
make check
make fetch-kernel
make kernel
make initramfs
make test-vm
make vm
make iso
```

`make fetch-kernel` checks the kernel.org release signature against Greg Kroah-Hartman's pinned signing-key fingerprint. The version comes from `config/kernel.version`. `make kernel` compiles `build/kernel-out/arch/x86/boot/bzImage`. `make initramfs` packages the static QauntumOS programs and required console device nodes into `build/qauntumos-initramfs.cpio.gz` without root privileges. `make test-vm` boots QEMU directly, checks `uname` and `version`, then powers off. `make vm` opens the interactive serial console. `make iso` creates `build/QauntumOS-Version-1-x86_64.iso` and its SHA-256 checksum. The build script uses installed Syslinux and GRUB files or copies placed under `build/host-tools/usr`.

For an existing ISO, boot it in QEMU with `qemu-system-x86_64 -m 1024 -cdrom build/QauntumOS-Version-1-x86_64.iso -boot d`. The shell appears on the VM display and serial console. BIOS and UEFI QEMU smoke tests passed locally with Linux 7.2.8 on 2026-10-02. No physical PC or handheld has been tested yet. Installation, persistent storage, graphics, controllers, and networking require later versions.
