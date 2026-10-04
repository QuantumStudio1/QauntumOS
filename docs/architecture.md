# Architecture decisions (draft)

## Boot path

The development build boots Linux 7.2.8 and an initramfs through ISOLINUX on BIOS or GRUB on UEFI, then starts `qauntum-init` (PID 1) and `qauntum-session`. The session presents profile creation on first boot, a graphical lock screen, and an early home screen. `qauntum.recovery=1` instead starts the recovery shell. The intended installed PC path is UEFI firmware → bootloader → Linux kernel + initramfs → `qauntum-init` → services → graphical session. A service supervisor and Unix user/session isolation come next. Secure Boot support follows reproducible images and key management.

## Profiles and persistence

Profiles are local records under `/var/lib/qauntumos/accounts`, with a random salt and PBKDF2-HMAC-SHA256 password hash. This is a prototype profile gate, not a Linux user account or a security boundary. On the live image the directory is in RAM and disappears at reboot. If the kernel command line explicitly supplies `qauntum.data=/dev/<partition>`, init mounts that ext4 partition at `/var/lib/qauntumos` and profiles persist. It never automatically mounts an unknown disk. A future installer will create and configure the data partition.

## System layout

Use a read-only system image for `/usr`, persistent `/var` for logs and package state, `/home` for user data, and a small writable `/etc` for local configuration. Start with ext4. Document migrations and backups before changing the layout. Games may live on a separate library volume that can use ext4, Btrfs, or a user-selected filesystem.

## Controller-first interface

The development lock screen draws directly to the Linux framebuffer and accepts keyboard or controller events. Its on screen keyboard supports D-pad movement, A to type, B to delete, and Start to confirm. The home screen is currently a placeholder. The target session should support controller navigation, a game library, launcher, storefront links, settings, downloads, performance overlay, sleep/resume, and accessibility. It should also work with keyboard, mouse, and touch. Users can open a regular desktop and install software outside the game library. Move the prototype shell to Wayland on an existing compositor before deciding whether a custom compositor is justified.

## Games and graphics

Build on upstream Linux graphics, Mesa, Vulkan, PipeWire, and existing game compatibility tools. The OS will own integration, defaults, and UX. Hardware testing must include AMD, Intel, and NVIDIA PCs plus representative handhelds; do not assume suspend, controllers, or GPU drivers behave identically.

## Updates from GitHub

`qauntum-update` will fetch a versioned release manifest and immutable image artifacts from this project's GitHub Releases. A GitHub repository change alone must not be executed as root on users' machines. The updater should verify a project signature and artifact hash, stage the new image in the inactive system slot, preserve `/home` and `/var`, then switch the boot target. A failed boot should roll back. The user-facing command can be `qauntum-update` or `qauntum update`.

Publishing to GitHub and building an updater require a repository URL, release key policy, CI build process, and recovery design. Those will be implemented before network updates are enabled.

## Trust and licensing

Pin upstream source versions and hashes. Track licenses and redistribution terms for the kernel, glibc, bootloader, firmware, drivers, and game-related components. Never bundle proprietary game clients or firmware without permission. Keep signing keys outside the public repository.
