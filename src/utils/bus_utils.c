#include "bus_utils.h"
#include <pwd.h>

static char xdg_runtime_dir[PATH_MAX + 1];
static char xauth_path[PATH_MAX + 1];

int bus_sender_fill_creds(sd_bus_message *m) {
    xdg_runtime_dir[0] = 0;
    xauth_path[0] = 0;
    
    sd_bus_creds *c = NULL;
    sd_bus_query_sender_creds(m, SD_BUS_CREDS_EUID, &c);
    if (c) {
        uid_t uid = 0;
        if (sd_bus_creds_get_euid(c, &uid) >= 0) {
            snprintf(xdg_runtime_dir, PATH_MAX, "/run/user/%d", uid);
            struct passwd *p = NULL;
            setpwent();
            for(p = getpwent(); p; p = getpwent()) {
                if (p->pw_uid == uid && p->pw_dir) {
                    snprintf(xauth_path, PATH_MAX, "%s/.Xauthority", p->pw_dir);
                    return 0;;
                }
            }
        }
    }
    return -1;
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
