

QauntumOS is an experimental gaming-focused Linux distribution for x86_64 PCs and handhelds. The long-term goal is a controller-first interface with the freedom of a general-purpose PC. The name is spelled **QauntumOS** throughout this repository.

## Initial design

- Linux kernel; x86_64; glibc; custom init and service manager.
- Pinned upstream Linux stable kernel: **7.2.8** (checked against kernel.org on 2026-10-02). The source is signature checked and built for the VM prototype.
- Build the distribution's integration, system services, update mechanism, interface, and filesystem **layout** in this repository.
- Start with established Linux filesystem drivers (for example ext4) and boot firmware interfaces. A new on-disk filesystem would be a separate research project and is not needed to own the OS design.
- Keep the underlying desktop accessible. The controller interface is the default session, with an option to enter a normal desktop and terminal.

## Current state

**Version 1 live ISO prototype:** Linux 7.2.8 boots from an ISO through BIOS or UEFI into QauntumOS's custom init and recovery shell. Both firmware paths passed a VM smoke test. This is a live recovery environment with no installer or persistent storage yet; the graphical interface and update command are also still in development.

```sh
make check
make fetch-kernel
make kernel
make initramfs
make test-vm
make iso
```

To enter the VM interactively, run `make vm`. The recovery shell supports `help`, `version`, `uname`, `ls`, `cat`, `echo`, `reboot`, and `poweroff`. The ISO is `build/QauntumOS-Version-1-x86_64.iso`. Build requirements include GCC with static glibc, make, Python 3, curl, xz, GnuPG, bc, flex, bison, Syslinux, GRUB, xorriso, and QEMU x86_64. The kernel source is downloaded and signature checked; generated files stay in the ignored `build/` directory. See [the build guide](docs/build.md).

See [the architecture](docs/architecture.md) and [the roadmap](docs/roadmap.md) for the build sequence.

## Source and contribution policy

The repository is intended to become the public source of the OS. External components, licenses, pinned versions, build recipes, and source URLs will be recorded before a distributable image is published. No claim is made that the Linux kernel, glibc, firmware, graphics drivers, or game compatibility layers were written by this project.
