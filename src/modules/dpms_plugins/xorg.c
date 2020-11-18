#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include "dpms.h"

DPMS("Xorg");

/*
 * info->power_level is one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 */
static int get(const char *display, const char *xauthority) {
    BOOL onoff;
    CARD16 s;
    int ret = WRONG_PLUGIN;
    
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSInfo(dpy, &s, &onoff);
            ret = s;
        } else {
            ret = UNSUPPORTED;
        }
        XCloseDisplay(dpy);
    }
    
    unsetenv("XAUTHORITY");
    return ret;
}

static int set(const char *display, const char *xauthority, int dpms_level) {
    int ret = WRONG_PLUGIN;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSEnable(dpy);
            DPMSForceLevel(dpy, dpms_level);
            XFlush(dpy);
            ret = 0;
        } else {
            ret = UNSUPPORTED;
        }
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}
