#ifdef SCREEN_PRESENT

#include <commons.h>
#include <X11/Xutil.h>

#define MONITOR_ILL_MAX              255

static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int getRootBrightness(const char *screen_name);

static const char object_path[] = "/org/clightd/clightd/Screen";
static const char bus_interface[] = "org.clightd.clightd.Screen";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetEmittedBrightness", "ss", "d", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

MODULE("SCREEN");

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

static int method_getbrightness(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    setenv("XAUTHORITY", xauthority, 1);
    const int br = getRootBrightness(display);
    unsetenv("XAUTHORITY");

    switch (br) {
    case -EINVAL:
        sd_bus_error_set_errno(ret_error, -br);
        return br;
    case 0:
        sd_bus_error_set_errno(ret_error, EIO);
        return -EIO;
    default:
        return sd_bus_reply_method_return(m, "d", (double)br / MONITOR_ILL_MAX);
    }
}

/* Robbed from calise source code, thanks!! */
static int getRootBrightness(const char *screen_name) {
    Display *dpy = XOpenDisplay(screen_name);
    if (!dpy) {
        return -EINVAL;
    }
    
    /* window frame size definition: 85% should be ok */
    const float pct = 0.85;
    int r = 0, g = 0, b = 0;
    int w = (int) (pct * XDisplayWidth(dpy, 0));
    int h = (int) (pct * XDisplayHeight(dpy, 0));
    int x = (XDisplayWidth(dpy, 0) - w) / 2;
    int y = (XDisplayHeight(dpy, 0) - h) / 2;
    
    Window root_window = XRootWindow(dpy, XDefaultScreen(dpy));
    XImage *ximage = XGetImage(dpy, root_window, x, y, w, h, AllPlanes, ZPixmap);
    if (ximage) {
        /*
         * takes 1 pixel every div*div area 
         * (div values > 8 will almost not give performance improvements)
         */
        const int div = 8;
        int wmax = (int) ((w/div) - 0.49);
        int hmax = (int) ((h/div) - 0.49);
        for (int i = 0; i < wmax; i++) {
            for (int k = 0; k < hmax; k++) {
                /* obtain r,g,b components from hex(p) */
                const int p = XGetPixel(ximage, div * i, div * k);
                r += ((int) p >> 16) & 0xFF;
                g += ((int) p >> 8) & 0xFF;
                b += ((int) p) & 0xFF;
            }
        }
        
        XDestroyImage(ximage);
        
        /* average r,g,b components and calculate px brightness on those values */
        const int area = (w/div)*(h/div);
        r = r/area;
        g = g/area;
        b = b/area;
    }
    XCloseDisplay(dpy);
    return (0.299 * r + 0.587 * g + 0.114 * b);
}

#endif
