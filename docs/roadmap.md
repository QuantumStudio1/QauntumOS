# Build roadmap

1. **Milestone 0 — source foundation (current):** architecture decisions and a compiling custom PID 1.
2. **Milestone 1 — boot in a VM:** pinned Linux 7.2.8 source (selected), kernel build, initramfs, bootloader, temporary shell/tooling, disk image build, and automated QEMU smoke test. This confirms the custom init actually boots.
3. **Milestone 2 — usable base:** custom service supervisor, login/session manager, networking, storage mounting, logs, package/build recipes, and installer. Replace temporary bootstrap utilities where useful.
4. **Milestone 3 — graphics and games:** Wayland session, controller-first shell, input mapping, audio, Steam/game launcher integration, performance profiles, and desktop escape hatch.
5. **Milestone 4 — release updates:** signed release manifests, A/B image installation, rollback, update command, and GitHub release pipeline.
6. **Milestone 5 — hardware release:** test matrix for PCs and handhelds, installer recovery, accessibility, documentation, and public beta.

Keep each milestone bootable and testable. A custom kernel, C library, GPU driver, or on-disk filesystem is outside the first release scope; these components can be studied or replaced independently later.
