# QauntumOS interface design

This is the target product interface for PCs and handhelds. The current framebuffer session is an early prototype; labels in this document do not imply that a feature already works.

## Visual identity

QauntumOS uses its own artwork and layout. The base palette is deep navy, electric cyan, and a restrained violet accent. Large readable type, clear focus outlines, generous spacing, and subtle motion should work at television distance. A profile can change colors, wallpaper, tile density, and motion. The default design must not copy Xbox, PlayStation, Steam, or Playnite artwork, icons, sounds, or screen layouts.

## Navigation model

The lock screen offers Console and Desktop before authentication. After login, both modes use the same local profile, game library, and settings. Switching modes should not require a reboot. The controller focus order must be predictable, visible, and usable without a mouse. Keyboard, mouse, and touch remain first-class inputs.

| Surface | Behavior required for first usable release |
| --- | --- |
| Home | Full-screen dashboard with Continue, Library, Storefronts, and a persistent quick-menu entry. Show installed games and recent activity without requiring a store account. |
| Library | Aggregate native games and optional store integrations. Search by title; filter by installed status, source, and favorites. Launch, favorite, inspect, and remove a game through clear actions. |
| Storefronts | Open installed clients such as Steam and optional other stores. Never require a particular storefront to use the OS or manually added games. |
| Quick menu | Accessible above a game or the dashboard. Show audio output and volume, display settings, performance profile, network status, and power actions. Only expose controls backed by working system services. |
| Desktop | A conventional Wayland desktop with windows, application launching, files, terminal, and settings. The current framebuffer desktop card is only a preview. |
| Customization | Save per-profile theme colors, wallpaper, dashboard layout, and widgets. Add a versioned plugin API after the core screens and permission model are stable. |
| System access | Provide files, terminal, settings, and explicit administrator actions. The console shell must not hide or block general-purpose PC access. |

## Controller contract

D-pad and left stick move focus; South confirms; East goes back; Start opens the quick menu; bumpers change major tabs. Each screen displays its current controls. Controller unplug/replug preserves focus and falls back to keyboard. Remapping, dead zones, long press, accessibility timing, and tests with Xbox, PlayStation, and common handheld controllers are required before claiming full controller support.

## Implementation order

1. Turn profile records into real isolated Linux user sessions and add a proper Wayland display stack.
2. Build a native library database and launcher for manually added native games; then add favorites, search, and filters.
3. Integrate Steam and other clients as optional sources, preserving the same library UI.
4. Implement working quick-menu services and the conventional desktop path.
5. Add per-profile customization, accessibility controls, and a documented plugin API.

Every feature should be testable with a keyboard and a controller. Keep incomplete controls clearly marked as previews; do not present decorative switches as working settings.
