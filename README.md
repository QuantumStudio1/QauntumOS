

QauntumOS is an experimental gaming-focused Linux distribution for x86_64 PCs and handhelds. The long-term goal is a controller-first interface with the freedom of a general-purpose PC. The name is spelled **QauntumOS** throughout this repository.

## Initial design

- Linux kernel; x86_64; glibc; custom init and service manager.
- Build the distribution's integration, system services, update mechanism, interface, and filesystem **layout** in this repository.
- Start with established Linux filesystem drivers (for example ext4) and boot firmware interfaces. A new on-disk filesystem would be a separate research project and is not needed to own the OS design.
- Keep the underlying desktop accessible. The controller interface is the default session, with an option to enter a normal desktop and terminal.

## Current state

This is version 0: a static custom init that mounts the basic virtual filesystems and launches a recovery shell. It is **not yet a bootable distribution image**. The shell and remaining userspace will be supplied in the next version. The host build check verifies compilation, not boot behavior.

```sh
make check
```

See [the architecture](docs/architecture.md) and [the roadmap](docs/roadmap.md) for the build sequence.

## Source and contribution policy

The repository is intended to become the public source of the OS. External components, licenses, pinned versions, build recipes, and source URLs will be recorded before a distributable image is published. No claim is made that the Linux kernel, glibc, firmware, graphics drivers, or game compatibility layers were written by this project.
