# Build and boot the Version 1 prototype

The current image boots directly in QEMU using its `-kernel` and `-initrd` options. It is a VM prototype, not an installer or bootable USB image.

## Host requirements

- x86_64 Linux host with GCC, static glibc, make, Python 3, curl, xz, and GnuPG.
- Kernel build tools: bc, flex, bison, OpenSSL development headers, ELF development headers, and a C toolchain.
- QEMU's `qemu-system-x86_64` for VM booting.
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
```

`make fetch-kernel` checks the kernel.org release signature against Greg Kroah-Hartman's pinned signing-key fingerprint. The version comes from `config/kernel.version`. `make kernel` compiles `build/kernel-out/arch/x86/boot/bzImage`. `make initramfs` packages the static QauntumOS programs and required console device nodes into `build/qauntumos-initramfs.cpio.gz` without root privileges. `make test-vm` boots QEMU, checks `uname` and `version`, then powers off. `make vm` opens the interactive serial console.

The VM test passed locally with Linux 7.2.8 on 2026-10-02. It does not test installation, storage persistence, graphics, controllers, networking, or real hardware. These require later versions.
