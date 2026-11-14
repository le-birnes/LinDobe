# Composition dirty rectangles

`dirty-composition.c` reproduces stale pixels outside Present1 dirty rectangles
on Wine's builtin D3D11 composition path. It checks two rotating back buffers by
default; pass `3` to test three. It also checks multiple regions, full Present
followed by partial Present1, rejected resizing with a held buffer, and a
successful size change. `2 --hwnd` checks ordinary HWND full-frame presentation.

Build with Wine's SDK headers (the MinGW C DirectComposition headers do not
provide the required interfaces):

```sh
/opt/adobewine/runtime/bin/winegcc -m64 -O1 -Wall -Werror \
  -o /tmp/dirty-composition.exe tests/dxgi/dirty-composition.c \
  -ld3d11 -ldxgi -ldxguid -luuid -lgdi32 -luser32
```

Run the resulting `/tmp/dirty-composition.exe.so` with an explicit disposable
prefix and runtime, using `WINEDLLOVERRIDES=dxgi,d3d11,d3d10core=b` and Wine's
Vulkan renderer. A separate Xvfb display is recommended: the test samples pixels
at its own window coordinates, so covering the window invalidates the result.
It does not control or inspect Adobe windows.

Original 0.6.1: dirty-region checks fail with stale red/blue pixels. Patch 0148:
two and three buffers pass, including resizing; ordinary HWND presentation also
passes. The test fails on setup errors. Windows and KDE references are pending.
