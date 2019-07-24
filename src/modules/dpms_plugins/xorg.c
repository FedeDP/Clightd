#ifdef DPMS_PRESENT

#include <commons.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

/*
 * info->power_level is one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 * 
 * Clightd returns -1 (DPMS_DISABLED) if dpms is disabled 
 * or an error if we're not on X
 */
int xorg_get_dpms_state(const char *display, const char *xauthority) {
    BOOL onoff;
    CARD16 s;
    int ret = -1;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSInfo(dpy, &s, &onoff);
            ret = s;
        }
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}

int xorg_set_dpms_state(const char *display, const char *xauthority, int dpms_level) {
    int ret = -1;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSEnable(dpy);
            DPMSForceLevel(dpy, dpms_level);
            XFlush(dpy);
            ret = 0;
        }
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}

void xorg_close(void) {
    /* Interface function */
}

#endif
