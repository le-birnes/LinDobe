# Photoshop Type Context Bar

Two separate Wine issues affected Photoshop's floating Type Context Bar.

- Font and size Edit controls contained valid values, but displayed no text.
  Choosing a size from the list worked; direct editing lacked visible text
  and selection feedback.
- The font picker could select a different row from the one under the cursor.
  Its native X11 position was 35 pixels below the position retained by Win32
  during an affected opening.

Both symptoms reproduced with the original runtime and with the independent
panel-window correction. The main toolbar and Character panel displayed the
same font and size correctly.

## Corrections

Patch 0141 allows visible, unmanaged popups to follow external X11 geometry
changes. It preserves the state/serial checks and excludes the desktop,
embedded windows, non-popup unmanaged windows and virtual desktop mode.
The font picker remains an override-redirect popup.

Patch 0142 gives fully opaque layered GDI children separate offscreen caches
when they are inside a per-pixel layered ancestor. Previously they inherited
the parent's dummy GDI surface: `GetClipBox` returned `NULLREGION` even though
the controls were visible and their Edit buffers and font handles were valid.
Removing `WS_EX_LAYERED` from just the controls did not solve that inheritance.

At presentation time, visible cached controls are composed into a temporary
copy of the parent's bitmap. The application's bitmap remains unchanged.
X11 uploads that temporary image without aliasing its shared backing image;
otherwise old control pixels would remain after a move or hide. A scaled
surface consumes the composed pixels once, before scaling to its target.

Window metadata and referenced child caches are collected before locking a
drawing surface. Pixel composition performs no window-tree queries. The
scaled target defers presentation callbacks until the outer surface has been
unlocked, preserving the user-lock/surface-lock order used by position updates.

The new path is limited to `LWA_ALPHA=255` children without a color key and
with a per-pixel ancestor. It does not implement general semi-transparent or
color-keyed child-window composition. Ordinary parent/control combinations
retain their previous drawing path. Shaped/overlapping controls and mixed-DPI
child hierarchies have not been validated; this is not a general child-layering
conformance claim.

No Adobe executable, font installation, compositor rule, decoration preference
or DPI setting needs changing. The installed runtime and normal prefix were
preserved; Photoshop was tested in a separate prefix/runtime.

## Validation

`tests/layered/context-controls.c` creates its own layered popup and standard
Edit control. It reads pixels only from its own X11 window. It checks text
updates, repeated parent updates, resizing, control/container movement and
visibility, destruction, and concurrent GDI painting/control movement.
`--late-parent` creates the controls before the
parent's first per-pixel update. A second case moves its own override-redirect
popup through Xlib and compares native and Win32 geometry.

- Original Wine 11.20 reproduces empty child drawing and stale popup geometry.
- The candidate passes both creation orders under Xvfb. Under virtual
  KWin 6.8.2 / XWayland 24.1.15 with software rendering, all four combinations
  of creation order and 96/144 DPI pass, including concurrent painting/movement.
- A disposable-prefix Xvfb run passes the rendering/lifecycle checks at
  144 DPI. These settings were confined to that test prefix.
- The user confirmed visible font and size values, direct size entry, correct
  font selection and continued interaction after switching focus in Photoshop
  2026 under Hyprland 0.57.2 / XWayland.

These KWin checks use the minimal program, not Photoshop on the maintainer's
KDE installation. Native Wine Wayland and Windows reference runs have not been
performed. No production package was built or installed.
