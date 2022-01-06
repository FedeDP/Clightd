#include "screen.h"
#include "bus_utils.h"
#include "xorg_utils.h"
#include <X11/Xutil.h>

SCREEN("Xorg");

/* Robbed from calise source code, thanks!! */
static int get_frame_brightness(const char *id, const char *env) {
    Display *dpy = fetch_xorg_display(&id, env);
    if (!dpy) {
        return WRONG_PLUGIN;
    }
    
    int ret = UNSUPPORTED;
    /* window frame size definition: 85% should be ok */
    const float pct = 0.85;
    int w = (int) (pct * XDisplayWidth(dpy, 0));
    int h = (int) (pct * XDisplayHeight(dpy, 0));
    int x = (XDisplayWidth(dpy, 0) - w) / 2;
    int y = (XDisplayHeight(dpy, 0) - h) / 2;
    
    Window root_window = XRootWindow(dpy, XDefaultScreen(dpy));
    XImage *ximage = XGetImage(dpy, root_window, x, y, w, h, AllPlanes, ZPixmap);
    if (ximage) {
        ret = rgb_frame_brightness((const uint8_t *)ximage->data, ximage->width, ximage->height, ximage->width);
        XDestroyImage(ximage);
    }
    XCloseDisplay(dpy);
    return ret;
}
