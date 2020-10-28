#if defined GAMMA_PRESENT || defined DPMS_PRESENT

#include "wl_utils.h"
#include "commons.h"
#include <module/map.h>

static void display_dtor(void *data);

static map_t *wl_map;

static void _ctor_ init_wl_map(void) {
    wl_map = map_new(true, display_dtor);
}

static void _dtor_ dtor_wl_map(void) {
    map_free(wl_map);
}

static void display_dtor(void *data) {
    wl_display_disconnect(data);
}

struct wl_display *fetch_wl_display(const char *display, const char *env) {
    struct wl_display *dpy = map_get(wl_map, display);
    if (!dpy) {
        /* Required for wl_display_connect */
        setenv("XDG_RUNTIME_DIR", env, 1);
        dpy = wl_display_connect(display);
        unsetenv("XDG_RUNTIME_DIR");
        
        if (dpy) {
            map_put(wl_map, display, dpy);
        }
    }
    return dpy;
}

#endif
