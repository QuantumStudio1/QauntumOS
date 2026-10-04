# Architecture decisions (draft)

## Boot path

The development build boots Linux 7.2.8 and an initramfs through ISOLINUX on BIOS or GRUB on UEFI, then starts `qauntum-init` (PID 1) and `qauntum-session`. The session presents profile creation on first boot, a graphical lock screen, and an early home screen. `qauntum.recovery=1` instead starts the recovery shell. The intended installed PC path is UEFI firmware → bootloader → Linux kernel + initramfs → `qauntum-init` → services → graphical session. A service supervisor and Unix user/session isolation come next. Secure Boot support follows reproducible images and key management.

## Profiles and persistence

Profiles are local records under `/var/lib/qauntumos/accounts`, with a random salt and PBKDF2-HMAC-SHA256 password hash. This is a prototype profile gate, not a Linux user account or a security boundary. On the live image the directory is in RAM and disappears at reboot. If the kernel command line explicitly supplies `qauntum.data=/dev/<partition>`, init mounts that ext4 partition at `/var/lib/qauntumos` and profiles persist. It never automatically mounts an unknown disk. A future installer will create and configure the data partition.

## System layout

Use a read-only system image for `/usr`, persistent `/var` for logs and package state, `/home` for user data, and a small writable `/etc` for local configuration. Start with ext4. Document migrations and backups before changing the layout. Games may live on a separate library volume that can use ext4, Btrfs, or a user-selected filesystem.

## Controller-first interface

The development lock screen draws directly to the Linux framebuffer and accepts keyboard or controller events. Before unlock, Tab or either controller bumper switches between Console and Desktop. Its on screen keyboard supports D-pad movement, A to type, B to delete, and Start to confirm. Device discovery repeats while the screen is open, so a newly connected controller can be used. The desktop choice currently opens a distinct workspace preview with the same limited cards as the console home; it is not yet a general purpose desktop or a display server. The game library now stores local executable entries per profile, supports search and favorites, and exposes installed filtering. A standalone command can launch a game as a regular Linux user without a shell. The live session runs as root, so launching from that session is disabled until Unix user isolation is implemented. Move the prototype shell to Wayland on an existing compositor before deciding whether a custom compositor is justified.

## Games and graphics

Build on upstream Linux graphics drivers rather than editing them to claim a different distro. The kernel should provide AMDGPU, i915/xe, Nouveau, and simpledrm as appropriate; package Mesa, libdrm, Vulkan loader and ICDs, firmware, and hardware discovery into the eventual root filesystem. NVIDIA's proprietary driver needs a separate redistributability and kernel compatibility decision. This framebuffer preview does not ship a working accelerated userspace graphics stack. The OS will own integration, defaults, and UX. Hardware testing must include AMD, Intel, and NVIDIA PCs plus representative handhelds; do not assume suspend, controllers, or GPU drivers behave identically.

## Game library

[Playnite](https://github.com/JosefNemec/Playnite) is a useful model for aggregating storefronts, local games, metadata, and a controller-friendly fullscreen view, but its released application uses Windows UI technology and cannot be made Linux native through a theme change. The current development image does not bundle Playnite or launch games yet. The planned QauntumOS library will implement Linux-native game discovery and launch, with optional Steam and other integrations, and preserve license attribution for any Playnite code actually reused. A future compatibility layer could run Windows Playnite separately, but it would not replace a native library or provide a reliable system login surface.

## Updates from GitHub

`qauntum-update` will fetch a versioned release manifest and immutable image artifacts from this project's GitHub Releases. A GitHub repository change alone must not be executed as root on users' machines. The updater should verify a project signature and artifact hash, stage the new image in the inactive system slot, preserve `/home` and `/var`, then switch the boot target. A failed boot should roll back. The user-facing command can be `qauntum-update` or `qauntum update`.

Publishing to GitHub and building an updater require a repository URL, release key policy, CI build process, and recovery design. Those will be implemented before network updates are enabled.

## Trust and licensing

Pin upstream source versions and hashes. Track licenses and redistribution terms for the kernel, glibc, bootloader, firmware, drivers, and game-related components. Never bundle proprietary game clients or firmware without permission. Keep signing keys outside the public repository.
