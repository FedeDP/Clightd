#pragma once

#include <X11/Xlib.h>

Display *fetch_xorg_display(const char **display, const char *xauthority);
