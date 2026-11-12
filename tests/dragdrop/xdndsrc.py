#!/usr/bin/env python3
"""xdndsrc.py FILE: a GTK3 X11 window at (0,0) 200x200 that offers FILE as text/uri-list when dragged (for the
dragdrop tests: xdotool presses inside it and drags onto the Wine window)."""
import sys, gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk
uri = 'file://' + sys.argv[1]
w = Gtk.Window(title='xdndsrc'); w.move(0, 0); w.set_default_size(200, 200)
b = Gtk.EventBox(); b.add(Gtk.Label(label='drag me')); w.add(b)
b.drag_source_set(Gdk.ModifierType.BUTTON1_MASK, [Gtk.TargetEntry.new('text/uri-list', 0, 0)], Gdk.DragAction.COPY)
b.connect('drag-data-get', lambda wi, ctx, sel, info, t: sel.set_uris([uri]))
w.connect('destroy', Gtk.main_quit); w.show_all(); Gtk.main()
