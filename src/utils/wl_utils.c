#if defined GAMMA_PRESENT || defined DPMS_PRESENT || defined SCREEN_PRESENT

#include "wl_utils.h"
#include "commons.h"
#include <module/map.h>

typedef struct {
    struct wl_display *dpy;
    char *env;
} wl_info;

static void wl_info_dtor(void *data);

static map_t *wl_map;

static void _ctor_ init_wl_map(void) {
    wl_map = map_new(true, wl_info_dtor);
}

static void _dtor_ dtor_wl_map(void) {
    map_free(wl_map);
}

static void wl_info_dtor(void *data) {
    wl_info *info = (wl_info *)data;
    wl_display_disconnect(info->dpy);
    free(info->env);
    free(info);
}

struct wl_display *fetch_wl_display(const char *display, const char *env) {
    if (env) {
        wl_info *info = map_get(wl_map, display);
        if (!info) {
            /* Required for wl_display_connect */
            setenv("XDG_RUNTIME_DIR", env, 1);
            struct wl_display *dpy = wl_display_connect(display);
            unsetenv("XDG_RUNTIME_DIR");
            
            if (dpy) {
                info = malloc(sizeof(wl_info));
                if (info) {
                    info->dpy = dpy;
                    info->env = strdup(env);
                    map_put(wl_map, display, info);
                } else {
                    fprintf(stderr, "Failed to malloc.\n");
                    wl_display_disconnect(dpy);
                }
            }
        }

        /* 
        * Actually check that env passed is the same that
        * was stored when wl_display connection was created
        */
        if (info && !strcmp(info->env, env)) {
            return info->dpy;
        }
    }
    return NULL;

}

int create_anonymous_file(off_t size, const char *filename) {
    int fd = memfd_create(filename, 0);
    if (fd < 0) {
        return -1;
    }

    int ret;
    do {
        errno = 0;
        ret = ftruncate(fd, size);
    } while (errno == EINTR);
    if (ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

#endif
