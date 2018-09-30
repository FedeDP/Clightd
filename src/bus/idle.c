#ifdef IDLE_PRESENT

#include <idle.h>
#include <X11/extensions/scrnsaver.h>

static time_t get_idle_time(const char *display, const char *xauthority);

static time_t get_idle_time(const char *display, const char *xauthority) {
    time_t idle_time;
    static XScreenSaverInfo *mit_info;
    Display *dpy;
    int screen;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    mit_info = XScreenSaverAllocInfo();
    if (!(dpy = XOpenDisplay(display))) {
        idle_time = -ENXIO;
        goto end;
    }
    screen = DefaultScreen(dpy);
    XScreenSaverQueryInfo(dpy, RootWindow(dpy,screen), mit_info);
    idle_time = mit_info->idle;
    XFree(mit_info);
    XCloseDisplay(dpy);
    
end:
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return idle_time;
}

int method_get_idle_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int idle_t = get_idle_time(display, xauthority);
    if (idle_t == -ENXIO) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return -ENXIO;
    }
    
    printf("Idle time: %dms\n", idle_t);
    return sd_bus_reply_method_return(m, "i", idle_t);
}

#endif
