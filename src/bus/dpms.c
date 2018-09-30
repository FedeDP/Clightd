#ifdef DPMS_PRESENT

#include <dpms.h>
#include <polkit.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#define DPMS_DISABLED -1
#define NO_X -2

struct dpms_timeout {
    CARD16 standby;
    CARD16 suspend;
    CARD16 off;
};

static int get_dpms_state(const char *display, const char *xauthority);
static int set_dpms_state(const char *display, const char *xauthority, int dpms_level);
static int get_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t);
static int set_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t);

int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int dpms_state = get_dpms_state(display, xauthority);
    if (dpms_state == NO_X) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return NO_X;
    }
    
    if (dpms_state == DPMS_DISABLED) {
        printf("Dpms is currently disabled.\n");
    } else {
        printf("Current dpms state: %d\n", dpms_state);
    }
    return sd_bus_reply_method_return(m, "i", dpms_state);
}

int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    int level;
    
    /* Require polkit auth */
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi", &display, &xauthority, &level);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (level < DPMS_DISABLED || level > DPMSModeOff) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong DPMS level value.");
        return -EINVAL;
    }
    
    int err = set_dpms_state(display, xauthority, level);
    if (err) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return err;
    }
    
    if (level != DPMS_DISABLED) {
        printf("New dpms state: %d\n", level);
    } else {
        printf("Dpms disabled.\n");
    }
    return sd_bus_reply_method_return(m, "i", level);
}

int method_getdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    struct dpms_timeout t = {0};
    int error = get_dpms_timeouts(display, xauthority, &t);
    if (error == NO_X) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return NO_X;
    }
    if (error == DPMS_DISABLED) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Dpms is disabled.");
        return DPMS_DISABLED;
    }
    
    printf("Current dpms timeouts:\tStandby: %ds\tSuspend: %ds\tOff:%ds.\n", t.standby, t.suspend, t.off);
    return sd_bus_reply_method_return(m, "iii", t.standby, t.suspend, t.off);
}

int method_setdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Require polkit auth */
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    int standby, suspend, off;
    int r = sd_bus_message_read(m, "ssiii", &display, &xauthority, &standby, &suspend, &off);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (standby < 0 || suspend < 0 || off < 0) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong DPMS timeouts values.");
        return -EINVAL;
    }
    
    struct dpms_timeout t = { .standby = standby, .suspend = suspend, .off = off };
    int err = set_dpms_timeouts(display, xauthority, &t);
    if (err) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return err;
    }
    
    printf("New dpms timeouts:\tStandby: %ds\tSuspend: %ds\tOff:%ds.\n", t.standby, t.suspend, t.off);
    return sd_bus_reply_method_return(m, "iii", standby, suspend, off);
}

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
    int ret = NO_X;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);

    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSInfo(dpy, &s, &onoff);
            if (onoff) {
                ret = s;
            } else {
                ret = DPMS_DISABLED;
            }
        }
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}

int set_dpms_state(const char *display, const char *xauthority, int dpms_level) {
    int ret = 0;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            if (dpms_level != DPMS_DISABLED) {
                DPMSEnable(dpy);
                DPMSForceLevel(dpy, dpms_level);
            } else {
                DPMSDisable(dpy);
            }
            XFlush(dpy);
        }
        XCloseDisplay(dpy);
    } else {
        ret = NO_X;
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}

int get_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t) {
    BOOL onoff;
    CARD16 s;
    int ret = NO_X;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSInfo(dpy, &s, &onoff);
            if (onoff) {
                DPMSGetTimeouts(dpy, &(t->standby), &(t->suspend), &(t->off));
                ret = 0;
            } else {
                ret = DPMS_DISABLED;
            }
        }
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}

int set_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t) {
    int ret = 0;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy) {
        if (DPMSCapable(dpy)) {
            DPMSEnable(dpy);
            DPMSSetTimeouts(dpy, t->standby, t->suspend, t->off);
            XFlush(dpy);
        } else {
            ret = NO_X;
        }
        XCloseDisplay(dpy);
    }

    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return ret;
}

#endif
