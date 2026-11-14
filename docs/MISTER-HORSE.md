# Mister Horse on LinDobe

Product Manager can install Animation Composer and its free content in the
same LinDobe prefix as After Effects. Browser login, preset application,
playback, saving, reopening and a short render passed first in an isolated
prefix, then in the normal prefix with a runtime built from main.

This is experimental support for the versions below. Other plugin vendors remain
untested. The initial investigation did not test Premiere plugin workflows or
a Windows reference. The later Premiere investigation is described below.

## Install and sign in

Use a LinDobe runtime containing patches **0134, 0135 and 0136**, built for
your CPU. The original 0.5.5 runtime does not contain these three patches;
the package version alone does not establish that they are included. The later
Product Manager redraw and Premiere installation corrections additionally require
**0148/0149** and the launcher changes described in the follow-up section.

1. Prepare LinDobe normally with `adobewine setup` and `adobewine launchers`.
   When upgrading an existing installation, run these commands from the version
   containing this integration to refresh the prefix settings and desktop entries.
2. Download the official Windows Product Manager MSI from
   <https://misterhorse.com/downloads/product-manager/win> and install it:

   ```sh
   adobewine run msiexec /i /absolute/path/to/ProductManager.msi
   ```

3. Start the installed manager:

   ```sh
   adobewine run 'C:\Program Files\Mister Horse Product Manager\ProductManager.exe'
   ```

4. Choose **Log in with browser**, sign in yourself and accept the browser's
   request to return to **Mister Horse Product Manager (LinDobe)**. Do not share
   callback links: they can contain authentication data.
5. Install Animation Composer and free content for After Effects from Product
   Manager, following
   [Mister Horse's workflow](https://help.misterhorse.com/hc/en-us/articles/4405555986321-How-to-install-Animation-Composer).
6. Open After Effects normally and open Animation Composer from its Extensions menu.

`setup` applies these settings only to `ProductManager.exe`:

| AppDefaults subkey | Values (REG_SZ) |
|---|---|
| `DllOverrides` | `dxgi=builtin`, `d3d11=builtin`, `d3d10core=builtin` |
| `Direct3D` | `renderer=vulkan` |

`launchers` registers `adobewine-misterhorse.desktop` for
`x-scheme-handler/misterhorsepm`, even before the manager is installed. The
entry forwards one URI through the existing `adobewine run` command, preserving
the prefix and LinDobe home used to generate it. It also honors `ADOBEWINE_EXEC`
for the configured Flatpak command. There is no new plugin-manager command.
The entry is hidden from the application menu until Product Manager is installed;
rerun `adobewine launchers` after installing it to show it in the menu. The same
entry handles browser callbacks and does not replace existing MIME preferences.
If another application already handles this scheme, select
the LinDobe handler in the browser's application chooser.

Product Manager must use the default installer location,
`C:\Program Files\Mister Horse Product Manager\ProductManager.exe`, in that
same prefix. No development DLL copies or environment overrides are needed.

## Tested setup

| Component | Version or configuration |
|---|---|
| Date | 2026-11-08 to 2026-11-09 |
| Runtime | Wine 11.20, `/opt/adobewine/runtime` |
| Product Manager | 3.7.7, JUCE 8.0.10 |
| Animation Composer | 4.2.6 |
| After Effects | 26.5.0.89 |
| Desktop | CachyOS, Hyprland through XWayland |
| Hardware | Ryzen 5 5600X, NVIDIA RTX 3060 |
| Initial investigation base | `3aaea375bc011bc23a51ccf4edc8711cb9b98eee` |
| Installed main runtime | `ae7eed971b21a5ac0269fc48ea0a70079a4fa269`, generic x86-64 build |

The official MSI came from
<https://misterhorse.com/downloads/product-manager/win> and has SHA-256
`bbc4b34ad4ce30cfc4b06e970bd6c851cd6125aced36d17bea35910045d69b1d`.
Installation used Product Manager, following
[Mister Horse's documented workflow](https://help.misterhorse.com/hc/en-us/articles/4405555986321-How-to-install-Animation-Composer).

## Validation history

The initial investigation used a separate copy of the working Adobe prefix,
with explicit test `WINEPREFIX` and runtime `PATH`. The final installed-runtime
checks used the normal prefix. The plugin integration adds no global DLL,
renderer, file-dialog or window-decoration settings.

The user entered credentials in the browser/application and installed Animation
Composer and free content through Product Manager. The installed files include:

- `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MisterHorse\AnimationComposer4\AnimationComposer4.aex`
- `C:\Program Files\Common Files\Adobe\CEP\extensions\com.misterhorse.AnimationComposer`

The original investigation registered the Linux protocol handler manually.
The launcher generator now creates it as part of normal desktop integration.

## Required corrections

### Product Manager startup and interaction

DXVK returns `E_NOTIMPL` for the composition swapchain used by JUCE. Wine's
DXGI supports that path through patches 0050/0051, but its `WaitForVBlank`
stub prevents JUCE's VBlankThread from completing shutdown during window
construction. Patch **0134** connects the wait to the existing D3DKMT
implementation from 0047. The standalone test changes from immediate
`E_NOTIMPL` on both attached outputs to successful waits.

Product Manager uses Wine's builtin DXGI, D3D11 and D3D10Core with its Vulkan
renderer. The OpenGL comparison had incorrect presentation and an invisible
cursor. A native debugger also caught a GFNI instruction in the original
0.5.5 `winex11.so`, unsupported by this test CPU. That release had been built
with `-march=native`; the maintainer rebuilt it for generic x86-64. The normal
prefix was subsequently validated without the earlier per-app
`ClientSideWithRender=N` workaround. The integration does not set that value.

The final development cold launch retained the account session and installed
package with correct drawing. In the installed main runtime, the user reported
a nonblocking redraw glitch on the first launch, then normal interaction.
Earlier hover artifacts were not isolated to one specific correction; no dirty
rectangle or single-buffer experiment is part of this integration.

Patch 0134 inherits 0047's refresh-based pacing, not hardware vblank
synchronization. That implementation uses the primary display's refresh
rate rather than the requested output's rate.

### Animation Composer loading

Animation Composer imports `KERNEL32.ReadDirectoryChangesExW`, missing in the
original runtime. It requests extended notifications and has an existing
fallback to `ReadDirectoryChangesW` when the request fails. Patch **0135**
implements the basic information class and rejects the unsupported extended
format with `ERROR_INVALID_FUNCTION`, allowing that fallback without returning
incorrectly formatted records. It does not claim full extended-notification
support. The asynchronous notification test passes, and After Effects starts
and loads the plugin instead of aborting on the missing import.

### After Effects viewer presentation

The viewer flicker also reproduces without Mister Horse, with the original
core DLLs, and with hardware-accelerated viewers disabled before a cold start.
Traces identify D3D11/DXVK and D3D12/vkd3d surfaces targeting the same viewer
HWND. Temporarily suppressing either source distinguishes the black covering
surface from the actual composition. No source-ID filter is retained.

Patch **0136** recognizes vkd3d's Vulkan engine name. After a successful
presentation, a private window property retains that client surface as the
HWND's presentation owner; competing client-surface copies are skipped until
the owning swapchain/surface is destroyed. The graphics-driver ABI is
unchanged. The mixed D2D/D3D12 probe confirms both retained display and return
to ordinary D2D drawing after release.

This is a targeted vkd3d compatibility rule. Microsoft discourages mixing
flip presentation with other APIs in one HWND, so it is not presented as a
general multi-API composition implementation or a Windows conformance fix.
The maintainer subsequently reported passing After Effects (5/5), Premiere
(7/7), Photoshop (10/10) and standalone regression (28/28) suites with the three
patches in [the merged contribution](https://github.com/le-birnes/LinDobe/pull/33).

## Product Manager and Premiere follow-up (2026-11-14)

Product Manager 3.7.7 exhibited two additional problems on the official 0.6.1
runtime: switching between its After Effects and Premiere tabs mixed old and
new content, and installing Animation Composer for Premiere failed in UPIA.

### Partial composition updates

Product Manager uses `IDXGISwapChain1::Present1` with one or more dirty
rectangles. Wine ignored those parameters and presented the whole rotating
back buffer, including stale pixels outside the regions JUCE had redrawn.
Patch **0148** retains the previous composition, updates its dirty regions,
and copies the composed frame into the current back buffer before presenting.
A full `Present` or `Present1` replaces that history; successful `ResizeBuffers`
discards it. A rejected resize leaves it intact.

This path applies only to Wine's D3D11 composition swapchains, using one
additional device texture per swapchain. Ordinary HWND swapchains, DXVK and
D3D12 presentation keep their existing paths. Scroll rectangles retain their
previous unsupported behavior and reset the retained history. The observed
Product Manager calls contained no scroll parameters.

The standalone reproducer fails on 0.6.1 and passes with patch 0148 using two
and three back buffers, multiple dirty rectangles, alternating `Present` and
`Present1`, and both successful and rejected resizing. Ordinary HWND full-frame
presentation also passes. The user confirmed that tab switching, hovering and
window resizing no longer flicker on Hyprland/XWayland. KDE and Windows
reference runs remain pending.

The expected dirty-region behavior is described in Microsoft's
[DXGI presentation documentation](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements).

### Premiere UXP installation

The captured official package is Animation Composer for Premiere **2.2.3**,
plugin ID `com.misterhorse.animationcomposer.premiere.uxp`. Its manifest
requires Premiere 25.1.0 or later; the installed host is **26.5.2**.
Two independent path issues prevented the vendor installer from completing:

- From a Linux working directory on Wine's Z: drive, UPIA drops the drive
  letter from a user-data path while preparing directories and returns `-198`.
  The launcher now starts only the default installed Product Manager from its
  prefix's C: drive. File arguments are resolved before changing directory.
- Once that error is avoided, UPIA queries `CommonW6432Dir` in the native
  64-bit `HKLM\Software\Microsoft\Windows\CurrentVersion` key. Wine omits
  this value, and UPIA returns `-1` while constructing its common UXP path.
  Patch **0149** initializes the value from Wine's native common-files folder
  in the AMD64 and ARM64 registry sections. An existing customized value is
  preserved; both a fresh prefix and upgrading an older prefix passed, including
  preservation of a deliberately customized alias during `wineboot -u`. Microsoft
  shows the alias in its
  [CurrentVersion registry example](https://learn.microsoft.com/en-us/powershell/scripting/samples/working-with-registry-entries).

With the native path present and a C: working directory, the unmodified official
UPIA reports a successful installation. The installed manifest was verified in
`C:\Program Files\Common Files\Adobe\UXP\Plugins\External`.
The user confirmed the installed status in Product Manager and successfully
opened the plugin panel in Premiere, where it appeared under **Extensions**.

[Adobe's UPIA instructions](https://developer.adobe.com/premiere-pro/uxp/plugins/distribution/install/)
also run the Windows installer from its installed C: directory. The integration
uses the normal vendor installer and its validation. No plugin extraction or
manual database population is part of the correction.

Reproducers:

- `tests/dxgi/dirty-composition.c` and its README.
- `tests/misterhorse/native-common-files.c` checks the native registry aliases.
- `tests/launcher` checks Product Manager visibility, callbacks, working directory
  and preservation of relative file arguments; 21 tests pass.

## Acceptance evidence

| Check | Result |
|---|---|
| Product Manager content, input and browser login | User confirmed |
| Session and installed-package persistence after cold restart | User confirmed |
| Final Product Manager redraw | User confirmed correct |
| After Effects startup and Animation Composer panel | User confirmed |
| Free preset application and playback | User confirmed |
| Final viewer stability and panel mouse resizing | User confirmed normal |
| Save and reopen | `saveAndReopen=PASS`, three enabled expressions |
| Final short render | `render=PASS`, 1280x720, 24 fps, 3 seconds, 72 frames |
| Render content | 21 distinct frame hashes; all 72 hashes match the earlier render |
| Mixed D2D/D3D12 probe | Original: black then green; patched: red then green |
| Installed main runtime: panel, playback, viewer and resizing | User confirmed normal |
| Installed main runtime: save, reopen and 72-frame render | Passed; frame hashes match the isolated reference |
| Installed main runtime: Media Encoder handoff | Queued successfully; user started and completed encoding manually |

The final application checks use patch 0136 with the original X11 driver and
without the diagnostic source filter. Viewer acceleration, plugin files, and
other temporary comparison settings were restored. No screenshots or visual
automation tools were used; visible behavior was checked by the user. Render
validation uses ExtendScript, media metadata, and frame hashes.

Reproducers and acceptance scripts:

- `tests/vblank/dxgi.c`
- `tests/directory-notify/ex.c`
- `tests/d3d12child/d3d12child.c -underlay`
- `tests/misterhorse/setup.jsx` and `verify.jsx`

The automatic handler and launcher exit-grace behavior have separate regression
tests in `tests/launcher`. Those tests use disposable prefixes, synthetic URIs,
stub Wine and the desktop-entry parser; they do not repeat browser authentication
or establish Flatpak sandbox compatibility.

## Rollback

With Product Manager closed, remove only its three `DllOverrides` values and
`Direct3D\renderer` under
`HKCU\Software\Wine\AppDefaults\ProductManager.exe`, or restore their prior
values if customized before setup. Leave global Wine and Adobe settings alone.
Running setup again reinstates the supported configuration.

Remove `adobewine-misterhorse.desktop` from
`${XDG_DATA_HOME:-$HOME/.local/share}/applications` and refresh that directory
with `update-desktop-database` to unregister only the browser callback.
`adobewine launchers --remove` also removes it along with the other LinDobe
entries. Registration does not write `mimeapps.list`, so no user preference
needs to be undone. Existing manually created associations are outside this
automatic registration. Installing or removing the handler does not delete
authentication, plugins, content or projects.
