#ifdef DPMS_PRESENT

#include "../inc/dpms.h"

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
int get_dpms_state(const char *display, const char *xauthority) {
    BOOL onoff;
    CARD16 s;
    int ret = -2;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);

    Display *dpy = XOpenDisplay(display);
    if (dpy &&  DPMSCapable(dpy)) {
        DPMSInfo(dpy, &s, &onoff);
        if (onoff) {
            ret = s;
        } else {
            ret = DPMS_DISABLED;
        }
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    
    if (dpy) {
        XCloseDisplay(dpy);
    }
    
    return ret;
}

int set_dpms_state(const char *display, const char *xauthority, int dpms_level) {
    int ret = dpms_level;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy && DPMSCapable(dpy)) {
        DPMSEnable(dpy);
        DPMSForceLevel(dpy, dpms_level);
        XFlush(dpy);
    } else {
        ret = -3;
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    
    if (dpy) {
        XCloseDisplay(dpy);
    }
    
    return ret;
}

int get_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t) {
    BOOL onoff;
    CARD16 s;
    int ret = -2;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy &&  DPMSCapable(dpy)) {
        DPMSInfo(dpy, &s, &onoff);
        if (onoff) {
            DPMSGetTimeouts(dpy, &(t->standby), &(t->suspend), &(t->off));
            ret = 0;
        } else {
            ret = DPMS_DISABLED;
        }
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    
    if (dpy) {
        XCloseDisplay(dpy);
    }
    
    return ret;
}

int set_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t) {
    int ret = 0;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy && DPMSCapable(dpy)) {
        DPMSEnable(dpy);
        DPMSSetTimeouts(dpy, t->standby, t->suspend, t->off);
        XFlush(dpy);
    } else {
        ret = -3;
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    
    if (dpy) {
        XCloseDisplay(dpy);
    }
    
    return ret;
}

#endif
