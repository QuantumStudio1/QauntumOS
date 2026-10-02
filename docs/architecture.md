# Architecture decisions (draft)

## Boot path

The current VM prototype boots Linux 7.2.8 and an initramfs directly through QEMU, then starts `qauntum-init` (PID 1) and the recovery shell. The intended PC path is UEFI firmware → bootloader → Linux kernel + initramfs → `qauntum-init` → services → graphical session. A service supervisor and login/session manager come next. Secure Boot support follows reproducible images and key management.

## System layout

Use a read-only system image for `/usr`, persistent `/var` for logs and package state, `/home` for user data, and a small writable `/etc` for local configuration. Start with ext4. Document migrations and backups before changing the layout. Games may live on a separate library volume that can use ext4, Btrfs, or a user-selected filesystem.

## Controller-first interface

The default session should support controller navigation, a game library, launcher, storefront links, settings, downloads, performance overlay, sleep/resume, and accessibility. The interface should also work with keyboard, mouse, and touch. Users can open a regular desktop and install software outside the game library. Prototype the shell as a Wayland client on an existing compositor before deciding whether a custom compositor is justified.

## Games and graphics

Build on upstream Linux graphics, Mesa, Vulkan, PipeWire, and existing game compatibility tools. The OS will own integration, defaults, and UX. Hardware testing must include AMD, Intel, and NVIDIA PCs plus representative handhelds; do not assume suspend, controllers, or GPU drivers behave identically.

## Updates from GitHub

`qauntum-update` will fetch a versioned release manifest and immutable image artifacts from this project's GitHub Releases. A GitHub repository change alone must not be executed as root on users' machines. The updater should verify a project signature and artifact hash, stage the new image in the inactive system slot, preserve `/home` and `/var`, then switch the boot target. A failed boot should roll back. The user-facing command can be `qauntum-update` or `qauntum update`.

Publishing to GitHub and building an updater require a repository URL, release key policy, CI build process, and recovery design. Those will be implemented before network updates are enabled.

## Trust and licensing

Pin upstream source versions and hashes. Track licenses and redistribution terms for the kernel, glibc, bootloader, firmware, drivers, and game-related components. Never bundle proprietary game clients or firmware without permission. Keep signing keys outside the public repository.
