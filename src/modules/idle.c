#ifdef IDLE_PRESENT

#include <commons.h>
#include <module/module_easy.h>
#include <X11/extensions/scrnsaver.h>

static int method_get_idle_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static time_t get_idle_time(const char *display, const char *xauthority);

static const char object_path[] = "/org/clightd/clightd/Idle";
static const char bus_interface[] = "org.clightd.clightd.Idle";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetTime", "ss", "i", method_get_idle_time, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

MODULE("IDLE");

static void module_pre_start(void) {
    
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    int r = sd_bus_add_object_vtable(bus,
                                     NULL,
                                     object_path,
                                     bus_interface,
                                     vtable,
                                     NULL);
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
}

static void receive(const msg_t *msg, const void *userdata) {
    
}

static void destroy(void) {
    
}

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
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int idle_t = get_idle_time(display, xauthority);
    if (idle_t == -ENXIO) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return -ENXIO;
    }
    
    m_log("Idle time: %dms.\n", idle_t);
    return sd_bus_reply_method_return(m, "i", idle_t);
}

#endif
