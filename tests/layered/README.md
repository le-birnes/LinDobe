# Layered overlay mapping probe

`alpha-map.c` exercises Photoshop-style 256x256 layered, nonactivating,
click-through popup windows. Only a small green label has nonzero alpha.

```sh
x86_64-w64-mingw32-gcc -O1 -Wall -Wextra -o alpha-map.exe tests/layered/alpha-map.c -luser32 -lgdi32
adobewine run /absolute/path/to/alpha-map.exe
```

Without MinGW, an installed Wine SDK can build a Winelib version:

```sh
/opt/adobewine/runtime/bin/winegcc -m64 -O1 -Wall -Wextra \
  -o /tmp/alpha-map.exe tests/layered/alpha-map.c -luser32 -lgdi32
adobewine run /tmp/alpha-map.exe.so
```

The first phase paints before showing the popup. The second maps an empty
popup before painting the label. Both repeatedly move, update and recreate it.
The API checks verify that logical window dimensions stay 256x256 and that
transparent padding does not intercept hit testing. The paint-event count is
reported to help detect repaint loops.

Pass `--managed` to exercise a captioned, managed layered popup instead of the
click-through readout:

```sh
adobewine run /absolute/path/to/alpha-map.exe --managed
```

This mode paints an opaque popup body and creates a second ordinary window.
It verifies 36 focus round-trips between the ordinary windows, checks that the
popup label and body remain hit-testable above their owner, and retains the
256x256 geometry check. Both paint-before-map and paint-after-map phases run.
The default readout mode keeps its transparent-padding and click-through checks.

The default display is a small green label with transparent padding; managed
mode shows an opaque popup containing the label. The API
result does not establish that the compositor displayed the alpha correctly;
that requires a user's observation. The probe does not read screen pixels,
capture images, inject mouse input or modify an Adobe document.

The API checks passed under Xvfb with both the baseline and patched Wine
11.20 runtimes, with eight paint events in each run. Windows reference results
and non-X11 display-driver regression results have not been collected.

For patch 0140, the original 0138 runtime and the narrowed candidate were also
compared under virtual KWin 6.8.2 and XWayland 24.1.15 in an isolated Arch
container, using software rendering. All four cases passed: unmanaged mode
reported eight paint events in each runtime, and managed mode reported ten.
These are API regression checks, not visual Photoshop validation on KDE or
coverage of every KWin version and graphics driver.
