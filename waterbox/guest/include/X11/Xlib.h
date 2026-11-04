/* The guest has no X11. Cemu's headers name a few of Xlib's types (in
 * their GLX declarations, never used by this core); these are the names,
 * nothing more. */
#pragma once
typedef unsigned long XID;
typedef unsigned long Window, Pixmap, Font, Colormap, Cursor, Drawable, GContext, KeySym, Atom, VisualID, Time;
typedef struct _XDisplay Display;
typedef int Bool;
typedef int Status;
typedef struct { void *ext_data; VisualID visualid; int c_class; unsigned long red_mask, green_mask, blue_mask; int bits_per_rgb; int map_entries; } Visual;
#define True 1
#define False 0
#define None 0L
#define Success 0
