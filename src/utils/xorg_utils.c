#if defined GAMMA_PRESENT || defined DPMS_PRESENT || defined SCREEN_PRESENT

#include "bus_utils.h"
#include "xorg_utils.h"

#define XORG_DISPLAY_DEF ":0"

Display *fetch_xorg_display(const char **display, const char *xauthority) {
    if (!*display || display[0] == 0) {
        *display = XORG_DISPLAY_DEF;
    }
    if (!xauthority || xauthority[0] == 0) {
        xauthority = bus_sender_xauth();
    }
    
    if (xauthority) {
        setenv("XAUTHORITY", xauthority, 1);
        Display *dpy = XOpenDisplay(*display);
        unsetenv("XAUTHORITY");
        return dpy;
    }
    return NULL;
}

#endif
