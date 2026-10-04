# Build and boot the development system

The development ISO boots through BIOS and UEFI firmware into profile setup or the lock screen. It runs in memory and does not install to disk. The direct QEMU boot path remains available for quick development tests.

## Host requirements

- x86_64 Linux host with GCC, glibc, OpenSSL development headers, make, Python 3, curl, xz, and GnuPG.
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
make test-session
make vm
make iso
```

`make fetch-kernel` checks the kernel.org release signature against Greg Kroah-Hartman's pinned signing-key fingerprint. The version comes from `config/kernel.version`. `make kernel` compiles `build/kernel-out/arch/x86/boot/bzImage` with framebuffer and Xbox controller drivers. `make initramfs` packages QauntumOS programs, the session's shared libraries, and required console device nodes without root privileges. `make test-vm` checks the recovery shell; `make test-session` checks profile creation, a rejected password, unlock, and poweroff. `make vm` opens the interactive serial console. On a host with QEMU's GTK display support, `QAUNTUM_DISPLAY=gtk make vm` opens the graphical VM window. `make iso` creates the unreleased `build/QauntumOS-Version-2-dev-x86_64.iso` and its SHA-256 checksum. The build script uses installed Syslinux and GRUB files or copies placed under `build/host-tools/usr`.

For an existing ISO, boot it in QEMU with `qemu-system-x86_64 -m 1024 -cdrom build/QauntumOS-Version-2-dev-x86_64.iso -boot d`. The graphical profile and lock screens appear on the VM display; prompts are also mirrored to serial. Set `QAUNTUM_RECOVERY=1` when using `tools/run-vm.sh` to start the recovery shell.

Profiles on the live image are temporary. For development persistence, create an empty ext4 image and attach it explicitly:

```sh
truncate -s 64M build/qauntum-data.img
mkfs.ext4 -F -L QAUNTUMDATA build/qauntum-data.img
QAUNTUM_DATA_IMAGE="$PWD/build/qauntum-data.img" make vm
```

The VM helper adds `qauntum.data=/dev/vda` to the kernel command line. This is a dedicated test image, not a host disk. The account and lock screen passed BIOS and UEFI QEMU tests on 2026-10-04, and a profile survived a reboot with the data image. Controller event handling is implemented but physical controller and handheld testing remain. No installer, privilege isolated user accounts, game launching, networking, or update command is present yet.
