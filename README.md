

QauntumOS is an experimental gaming-focused Linux distribution for x86_64 PCs and handhelds. The long-term goal is a controller-first interface with the freedom of a general-purpose PC. The name is spelled **QauntumOS** throughout this repository.

## Initial design

- Linux kernel; x86_64; glibc; custom init and service manager.
- Pinned upstream Linux stable kernel: **7.2.8** (checked against kernel.org on 2026-10-02). The source is signature checked and built for the VM prototype.
- Build the distribution's integration, system services, update mechanism, interface, and filesystem **layout** in this repository.
- Start with established Linux filesystem drivers (for example ext4) and boot firmware interfaces. A new on-disk filesystem would be a separate research project and is not needed to own the OS design.
- Keep the underlying desktop accessible. The controller interface is the default session, with an option to enter a normal desktop and terminal.

## Current state

**Current development:** Linux 7.2.8 now boots into first time profile creation, a graphical lock screen, and an early home screen. The lock screen has an on screen keyboard for controller input. Profiles use salted PBKDF2 password hashes. Account data is temporary in live mode; an explicitly attached ext4 data disk preserves it across boots. Both BIOS and UEFI VM tests passed. The published [Version 1 prerelease](https://github.com/QuantumStudio1/QauntumOS/releases/tag/v1.0.0-alpha.1) still contains the earlier recovery shell build.

This is a development preview. A local game library now supports adding absolute executable paths, searching titles, marking favorites, and filtering installed games from the dashboard. The standalone `qauntum-library` command can launch games under a regular Linux user. The live boot session still runs as root, so game launch is blocked there until real Unix user isolation exists. This build does not provide an installer or network updates.

```sh
make check
make fetch-kernel
make kernel
make initramfs
make test-vm
make test-session
make test-library
make iso
```

To enter the VM interactively, run `make vm`. Set `QAUNTUM_RECOVERY=1` to boot the recovery shell. The unreleased development ISO is `build/QauntumOS-Version-2-dev-x86_64.iso`. Build requirements include GCC with glibc, OpenSSL development files, make, Python 3, curl, xz, GnuPG, bc, flex, bison, Syslinux, GRUB, xorriso, and QEMU x86_64. The kernel source is downloaded and signature checked; generated files stay in the ignored `build/` directory. See [the build guide](docs/build.md).

See [the interface design](docs/interface.md), [the architecture](docs/architecture.md), and [the roadmap](docs/roadmap.md) for the target experience and build sequence.

## Source and contribution policy

The repository is intended to become the public source of the OS. External components, licenses, pinned versions, build recipes, and source URLs will be recorded before a distributable image is published. No claim is made that the Linux kernel, glibc, firmware, graphics drivers, or game compatibility layers were written by this project.
