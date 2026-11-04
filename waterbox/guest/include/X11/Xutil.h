/* See Xlib.h: names only. */
#pragma once
#include <X11/Xlib.h>
typedef struct { Visual *visual; VisualID visualid; int screen; int depth; int c_class; unsigned long red_mask, green_mask, blue_mask; int colormap_size; int bits_per_rgb; } XVisualInfo;
