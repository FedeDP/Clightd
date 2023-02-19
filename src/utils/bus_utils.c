#include "bus_utils.h"
#include <pwd.h>

static char xdg_runtime_dir[PATH_MAX + 1];
static char xauth_path[PATH_MAX + 1];

int bus_sender_fill_creds(sd_bus_message *m) {
    xdg_runtime_dir[0] = 0;
    xauth_path[0] = 0;
    
    int ret = -1;
    sd_bus_creds *c = NULL;
    sd_bus_query_sender_creds(m, SD_BUS_CREDS_EUID, &c);
    if (c) {
        uid_t uid = 0;
        if (sd_bus_creds_get_euid(c, &uid) >= 0) {
            snprintf(xdg_runtime_dir, PATH_MAX, "/run/user/%d", uid);
            setpwent();
            for (struct passwd *p = getpwent(); p; p = getpwent()) {
                if (p->pw_uid == uid && p->pw_dir) {
                    snprintf(xauth_path, PATH_MAX, "%s/.Xauthority", p->pw_dir);
                    ret = 0;
                    break;
                }
            }
            endpwent();
        }
        free(c);
    }
    return ret;
}

const char *bus_sender_runtime_dir(void) {
    if (xdg_runtime_dir[0] != 0) {
        return xdg_runtime_dir;
    }
    return NULL;
}

const char *bus_sender_xauth(void) {
    if (xauth_path[0] != 0) {
        return xauth_path;
    }
    return NULL;
}

void make_valid_obj_path(char *storage, size_t size, const char *root, const char *basename) {
    /*
     * Substitute wrong chars, eg: dell::kbd_backlight -> dell__kbd_backlight
     * See spec: https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-marshaling-object-path
     */
    const char *valid_chars = "ABCDEFGHIJKLMNOPQRSTUWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
    
    snprintf(storage, size, "%s/%s", root, basename);
    
    char *path = storage + strlen(root) + 1;
    const int full_len = strlen(path);
    
    while (true) {
        int len = strspn(path, valid_chars);
        if (len == full_len) {
            break;
        }
        path[len] = '_';
    }
}
