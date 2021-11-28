#include "xorg_utils.h"
#include <X11/extensions/dpms.h>
#include "dpms.h"
#include "bus_utils.h"

DPMS("Xorg");

/*
 * info->power_level is one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 */
static int get(const char **display, const char *xauthority) {
    BOOL onoff;
    CARD16 s;
    int ret = WRONG_PLUGIN;
    
    Display *dpy = fetch_xorg_display(display, xauthority);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSInfo(dpy, &s, &onoff);
            ret = s;
        } else {
            ret = UNSUPPORTED;
        }
        XCloseDisplay(dpy);
    }    
    return ret;
}

static int set(const char **display, const char *xauthority, int dpms_level) {
    int ret = WRONG_PLUGIN;
    
    Display *dpy = fetch_xorg_display(display, xauthority);
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
    return ret;
}
