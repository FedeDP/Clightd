#ifdef DPMS_PRESENT

#include "../inc/dpms.h"
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

/*
 * Checks through libx11 if DPMS is enabled for this xscreen,
 * else disables this module
 */
int get_dpms_state(const char *display, const char *xauthority) {
    BOOL onoff;
    CARD16 s;
    int ret = -2;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);

    Display *dpy = XOpenDisplay(display);
    if (dpy && DPMSCapable(dpy)) {
        DPMSInfo(dpy, &s, &onoff);
        if (onoff) {
            ret = s;
        } else {
            ret = DPMS_DISABLED;
        }
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    
    return ret;
}

#endif
