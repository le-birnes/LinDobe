# Photoshop measurement overlays on XWayland

Photoshop's move, transform and Smart Guides readouts can have two distinct
artifacts: opaque black padding from an incompletely refreshed Wine backing
surface, and background blur applied by the compositor to the transparent
portion of the popup. The Windows popup can be 256x256 even when its actual
label occupies only about 70x35 pixels.

## Wine correction

Patches 0138 and 0140 request a full cached-surface exposure when an unmanaged
alpha whole-window receives `MapNotify`. Full alpha exposures invalidate the
cached shape, also for the target of a scaled surface. This makes the existing
patch 0108 upload the complete alpha buffer after the native window becomes
viewable.

The window-data lock is released before exposing the surface. Surface flushing
does not take that driver lock. There is no server grab, repaint timer,
application-specific environment variable or graphics-driver ABI change.
Ordinary rectangular expose events and non-alpha surface invalidation keep
their previous behavior. Logical Windows geometry and input regions are not
cropped to work around compositor effects.

The original diagnosis used an isolated reflink copy of the prefix and runtime.
A single authorized numerical read outside a live label returned `ff000000`
from its X11 backing pixmap, while the corresponding Wine source padding was zero-alpha.
No screenshot or image file was saved. Comparisons of GC clipping, transfer
method, request serialization and compositor animation effects were not retained.

## Managed panel regression

The original 0138 refreshed managed alpha windows as well as measurement
overlays. On Hyprland/XWayland, Photoshop's expandable panels then showed an
extra outline from a separate `OWL.ShadowView` window and could stop accepting
input after switching applications. The corresponding Illustrator panel also
uses `OWL.FrameDrawer`, but its shadow owner stayed hidden in the comparison.

An isolated test kept the Photoshop executable and all other patches unchanged.
With 0138, the extra outline reproduced; without 0138, it disappeared. The
interaction failure had occurred in the normal instance but did not reproduce
in that isolated control run, so the comparison establishes the outline
regression without separately proving the cause of every focus failure.

Patch 0140 limits the map-time refresh to unmanaged windows. Managed palettes
and their shadow owners keep their previous mapping behavior, while unmanaged
Smart Guides readouts retain the full refresh. The win32u shape correction,
window ownership, input regions and compositor configuration are unchanged.

## Hyprland background blur

The remaining blurred rectangle is a separate compositor effect. Disabling
blur removed it while leaving the black artifact unchanged before the Wine
correction. Hyprland 0.57's renderer handles native Wayland popups separately
from X11 windows, so its popup-blur preference does not cover these XWayland
windows. See the
[renderer implementation](https://github.com/hyprwm/Hyprland/blob/0aa6745f705f2c4b0a5004eb019ef76e5bbdb426/src/render/Renderer.cpp).

For a Hyprland Lua configuration, this rule disables blur only for untitled,
floating Photoshop XWayland windows, including these readouts:

```lua
hl.window_rule({
    match = {
        class = "^photoshop[.]exe$",
        title = "^$",
        float = true,
        xwayland = true,
    },
    no_blur = true,
})
```

Keep the rule in a user-owned configuration file rather than a file generated
by a desktop shell. It does not disable blur for Photoshop's titled main
window or other applications. Matching is based on those window properties,
not a Photoshop-specific identification of every Smart Guides popup.
Removing the rule and reloading the configuration restores the compositor's
normal blur policy. LinDobe setup does not install or modify Hyprland rules.

## Validation

The initial tests used Photoshop 2026, Wine 11.20, CachyOS, Hyprland 0.57.0
through XWayland, a Ryzen 5 5600X and an NVIDIA RTX 3060 on 2026-11-09.

- The user confirmed that the mapping-refresh candidate removed black boxes,
  including the initial flash, while retaining readable measurements.
- A cold launch with the final clean modules, without diagnostic controls,
  passed repeated move/transform and Smart Guides checks: no boxes and normal
  measurements, confirmed by the user.
- With the scoped Hyprland rule, the user confirmed that both kinds of boxes
  disappeared and the measurements stayed visible.
- Illustrator opened with the final modules and passed the user's move,
  transform and Smart Guides check without either artifact. The Photoshop-only
  blur rule was not extended to Illustrator.
- InDesign passed the user's move/resize and Smart Guides check without either
  artifact, also without a compositor rule for that application.
- `tests/layered/alpha-map.c` passed its API checks with both baseline and
  patched modules under Xvfb, with eight paint events in each run.
- Patch 0138 applies without fuzz on top of the preceding Wine 11.20 patches.

The Xvfb API checks do not substitute for compositor display validation, and
no Windows reference or broad Adobe conformance claim is made. The patched
modules were validated in the isolated runtime; the installed runtime was
not replaced during this investigation.

The panel follow-up used the same environment on 2026-11-10 with Dux Shell:

- Disabling `no_blur` did not remove the extra surface. The original
  `no_blur = true` rule was restored before the runtime comparison.
- With 0138 and 0140 in the isolated runtime, the user confirmed that Character
  and another panel had no extra layer and retained interaction across focus
  switches. Move/transform with Smart Guides also passed without black boxes.
- `tests/layered/alpha-map.c` passed under Xvfb with the narrowed refresh:
  geometry and transparent-padding hit testing passed, with eight paint events.
- The candidate Unix modules loaded from the isolated runtime with their hashes
  verified. Their dependencies and symbols resolved. The installed runtime
  was not changed, and no production package was built.
- An isolated Arch container ran virtual KWin 6.8.2, XWayland 24.1.15 and
  software rendering. The original runtime and candidate both passed the
  unmanaged readout probe (eight paint events) and the managed opaque popup
  probe (ten paint events), including 36 focus round-trips and label/body hit
  testing per managed run. See [the probe instructions](../tests/layered/README.md).
  This does not establish visual Photoshop correctness on KDE, test the
  repository's documented KWin 6.7.5 reference environment, or cover native X11 sessions.
