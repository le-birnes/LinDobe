#!/bin/bash
# run_dropdlg.sh RUNTIME PREFIX DISPLAY: public #43 test. Drags a file from a GTK X11 source onto dropdlg.exe on a hidden
# screen, then presses Enter on the dialog Drop opened. Prints dropdlg's PASS/FAIL line.
set -u
rt=$1 pfx=$2 d=$3
[ -n "$d" ] && [ "$d" != :0 ] || { echo "refusing display '$d'"; exit 2; }
here=$(cd "$(dirname "$0")" && pwd)
unset WAYLAND_DISPLAY; export DISPLAY=$d GDK_BACKEND=x11 WINEPREFIX=$pfx WINEDEBUG=-all  # never the user's Wayland screen
[ -d "$pfx" ] || { "$rt/bin/wine" wineboot -i >/dev/null 2>&1; "$rt/bin/wineserver" -w; }
echo hi > "$pfx/drop.txt"
python3 "$here/xdndsrc.py" "$pfx/drop.txt" 2>/dev/null & src=$!
"$rt/bin/wine" "$here/dropdlg.exe" > "$pfx/out.txt" 2>&1 & w=$!
sleep 6
# where the window manager put them
geo() { xdotool getwindowgeometry --shell "$(xdotool search --name "^$1\$" | head -1)"; }
eval "$(geo xdndsrc)"; sx=$((X+50)) sy=$((Y+50))
eval "$(geo dropdlg)"; tx=$((X+WIDTH/2)) ty=$((Y+HEIGHT/2))
echo "drag $sx,$sy -> $tx,$ty"
xdotool mousemove $sx $sy mousedown 1
for i in 1 2 3 4 5 6 7 8 9 10; do xdotool mousemove $((sx+(tx-sx)*i/10)) $((sy+(ty-sy)*i/10)); sleep 0.15; done
xdotool mousemove $((tx+3)) $ty
sleep 0.5; xdotool mouseup 1
sleep 3
box=$(xdotool search --name dropdlg-box | head -1); echo "dialog window: ${box:-none}"
if [ -n "$box" ]; then xdotool windowactivate --sync $box 2>/dev/null; sleep 0.5; xdotool key Return; fi
for i in $(seq 45); do grep -q -E "PASS|FAIL" "$pfx/out.txt" && break; sleep 1; done
cat "$pfx/out.txt"
kill $src 2>/dev/null; "$rt/bin/wineserver" -k 2>/dev/null; wait 2>/dev/null
