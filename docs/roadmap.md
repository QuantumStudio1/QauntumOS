# Build roadmap

1. **Version 0 — source foundation (complete):** architecture decisions and a compiling custom PID 1.
2. **Version 1 — live ISO prototype (in progress):** Linux 7.2.8 kernel build, custom init, custom recovery shell, initramfs, BIOS/UEFI bootable ISO, and automated QEMU smoke tests are working. Next: installable disk image and persistent storage.
3. **Version 2 — usable base:** custom service supervisor, login/session manager, networking, storage mounting, logs, package/build recipes, and installer. Replace temporary bootstrap utilities where useful.
4. **Version 3 — graphics and games:** Wayland session, controller-first shell, input mapping, audio, Steam/game launcher integration, performance profiles, and desktop escape hatch.
5. **Version 4 — release updates:** signed release manifests, A/B image installation, rollback, update command, and GitHub release pipeline.
6. **Version 5 — hardware release:** test matrix for PCs and handhelds, installer recovery, accessibility, documentation, and public beta.

Keep each version bootable and testable. A custom kernel, C library, GPU driver, or on-disk filesystem is outside the first release scope; these components can be studied or replaced independently later.
