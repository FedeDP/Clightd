#ifdef IDLE_PRESENT

#include <commons.h>
#include <sys/inotify.h>
#include <module/module_easy.h>
#include <module/map.h>
#include <X11/extensions/scrnsaver.h>
#include <linux/limits.h>
#include <math.h>

#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

typedef struct {
    bool in_use;                // Whether the client has already been requested by someone
    bool is_idle;               // Whether the client is in idle state
    bool running;               // Whether "Start" method has been called on Client
    char *authcookie;
    char *display;
    unsigned int timeout;
    unsigned int id;            // Client's id
    int fd;                     // Client's timer fd
    char *sender;               // BusName who requested this client
    char path[PATH_MAX + 1];    // Client's object path
    sd_bus_slot *slot;          // vtable's slot
} idle_client_t;

static map_ret_code dtor_client(void *client);
static map_ret_code leave_idle(void *userdata, void *client);
static time_t get_idle_time(const idle_client_t *c);
static map_ret_code find_free_client(void *out, void *client);
static idle_client_t *find_available_client(void);
static void destroy_client(idle_client_t *c);
static int method_get_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_rm_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_start_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_stop_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int set_timeout(sd_bus *b, const char *path, const char *interface, const char *property, 
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_prop(sd_bus *b, const char *path, const char *interface, const char *property, 
                    sd_bus_message *value, void *userdata, sd_bus_error *error);

static map_t *clients;
static int inot_fd;
static int inot_wd;
static const char object_path[] = "/org/clightd/clightd/Idle";
static const char bus_interface[] = "org.clightd.clightd.Idle";
static const char clients_interface[] = "org.clightd.clightd.Idle.Client";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetClient", NULL, "o", method_get_client, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("DestroyClient", "o", NULL, method_rm_client, SD_BUS_VTABLE_UNPRIVILEGED | SD_BUS_VTABLE_METHOD_NO_REPLY),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable vtable_clients[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Start", NULL, NULL, method_start_client, SD_BUS_VTABLE_UNPRIVILEGED | SD_BUS_VTABLE_METHOD_NO_REPLY),
    SD_BUS_METHOD("Stop", NULL, NULL, method_stop_client, SD_BUS_VTABLE_UNPRIVILEGED | SD_BUS_VTABLE_METHOD_NO_REPLY),
    SD_BUS_WRITABLE_PROPERTY("Display", "s", NULL, set_prop, offsetof(idle_client_t, display), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_WRITABLE_PROPERTY("AuthCookie", "s", NULL, set_prop, offsetof(idle_client_t, authcookie), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_WRITABLE_PROPERTY("Timeout", "u", NULL, set_timeout, offsetof(idle_client_t, timeout), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Idle", "b", 0),
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
    clients = map_new();
    map_set_dtor(clients, dtor_client);
    int r = sd_bus_add_object_vtable(bus,
                                     NULL,
                                     object_path,
                                     bus_interface,
                                     vtable,
                                     NULL);
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
    inot_fd = inotify_init();
    m_register_fd(inot_fd, true, NULL);
    inot_wd = -1;
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        idle_client_t *c = (idle_client_t *)msg->fd_msg->userptr;
        if (c && !c->is_idle) {
            uint64_t t;
            read(msg->fd_msg->fd, &t, sizeof(uint64_t));
            
            int idle_t = lround((double)get_idle_time(c) / 1000);
            c->is_idle = idle_t >= c->timeout;
            struct itimerspec timerValue = {{0}};
            if (c->is_idle) {
                sd_bus_emit_signal(bus, c->path, clients_interface, "Idle", "b", true);
                if (inot_wd == -1) {
                    inot_wd = inotify_add_watch(inot_fd, "/dev/input/", IN_ACCESS | IN_ONESHOT);
                }
            } else {
                timerValue.it_value.tv_sec = c->timeout - idle_t;
            }
            timerfd_settime(msg->fd_msg->fd, 0, &timerValue, NULL);
            m_log("Client %d -> Idle: %d\n", c->id, c->is_idle);
        } else {
            char buffer[BUF_LEN];
            int length = read(msg->fd_msg->fd, buffer, BUF_LEN);
            if (length > 0) {
                m_log("Leaving idle state.\n");
                inotify_rm_watch(msg->fd_msg->fd, inot_wd);
                inot_wd = -1;
                map_iterate(clients, leave_idle, NULL);
            }
        }
    }
}

static void destroy(void) {
    map_free(clients);
}

static map_ret_code leave_idle(void *userdata, void *client) {
    idle_client_t *c = (idle_client_t *)client;
    if (c->is_idle) {
        sd_bus_emit_signal(bus, c->path, clients_interface, "Idle", "b", false);
        c->is_idle = false;
        struct itimerspec timerValue = {{0}};
        timerValue.it_value.tv_sec = c->timeout;
        timerfd_settime(c->fd, 0, &timerValue, NULL);
    }
    return MAP_OK;
}

static map_ret_code dtor_client(void *client) {
    idle_client_t *c = (idle_client_t *)client;
    if (c->in_use) {
        destroy_client(c);
    }
    free(c);
    return MAP_OK;
}

static time_t get_idle_time(const idle_client_t *c) {
    time_t idle_time;
    Display *dpy;
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", c->authcookie, 1);
    
    if (!(dpy = XOpenDisplay(c->display))) {
        idle_time = -ENXIO;
    } else {
        XScreenSaverInfo mit_info;
        int screen = DefaultScreen(dpy);
        XScreenSaverQueryInfo(dpy, RootWindow(dpy, screen), &mit_info);
        idle_time = mit_info.idle;
        XCloseDisplay(dpy);
    }
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    return idle_time;
}

static map_ret_code find_free_client(void *out, void *client) {
    idle_client_t *c = (idle_client_t *)client;
    idle_client_t **o = (idle_client_t **)out;
    
    if (!c->in_use) {
        *o = c;
        m_log("Returning unused client %u\n", c->id);
        return MAP_FULL; // break iteration
    }
    return MAP_OK;
}

static idle_client_t *find_available_client(void) {
    idle_client_t *c = NULL;
    if (map_iterate(clients, find_free_client, &c) != MAP_FULL) {
        /* no unused clients found. */
        c = calloc(1, sizeof(idle_client_t));
        if (c) {
            c->id = map_length(clients);
            m_log("Creating client %u\n", c->id);
        }
    }
    return c;
}

static void destroy_client(idle_client_t *c) {
    free(c->sender);
    free(c->authcookie);
    free(c->display);
    c->slot = sd_bus_slot_unref(c->slot);
    m_log("Freeing client %u\n", c->id);
}

static idle_client_t *validate_client(const char *path, sd_bus_message *m, sd_bus_error *ret_error) {
    idle_client_t *c = map_get(clients, path);
    if (c && c->in_use && !strcmp(c->sender, sd_bus_message_get_sender(m))) {
        return c;
    }
    m_log("Failed to validate client.\n");
    sd_bus_error_set_errno(ret_error, EPERM);
    return NULL;
}

static int method_get_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    idle_client_t *c = find_available_client();
    if (c) {
        c->in_use = true;
        c->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        m_register_fd(c->fd, true, c);
        c->sender = strdup(sd_bus_message_get_sender(m));
        snprintf(c->path, sizeof(c->path) - 1, "%s/Client%u", object_path, c->id);
        map_put(clients, c->path, c, true, true);
        sd_bus_add_object_vtable(bus,
                                &c->slot,
                                c->path,
                                clients_interface,
                                vtable_clients,
                                c);
        return sd_bus_reply_method_return(m, "o", c->path);
    }
    sd_bus_error_set_errno(ret_error, ENOMEM);
    return -ENOMEM;
}

static int method_rm_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *obj_path = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "o", &obj_path);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    idle_client_t *c = validate_client(obj_path, m, ret_error);
    if (c) {
        /* You can only remove stopped clients */ 
        if (!c->running) {
            m_deregister_fd(c->fd);
            destroy_client(c);
            int id = c->id;
            memset(c, 0, sizeof(idle_client_t));
            c->id = id; // avoid zeroing client id
            return sd_bus_reply_method_return(m, NULL);
        }
        sd_bus_error_set_errno(ret_error, EINVAL);
    }
    return -sd_bus_error_get_errno(ret_error);
}

static int method_start_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    idle_client_t *c = validate_client(sd_bus_message_get_path(m), m, ret_error);
    if (c) {
        /* You can only start not-started clients, that must have Display, AuthCookie and Timeout setted */
        if (c->timeout > 0 && c->display && c->authcookie && !c->running) {
            struct itimerspec timerValue = {{0}};
            timerValue.it_value.tv_sec = c->timeout;
            timerfd_settime(c->fd, 0, &timerValue, NULL);
            c->running = true;
            m_log("Starting Client %u\n", c->id);
            return sd_bus_reply_method_return(m, NULL);
        }
        sd_bus_error_set_errno(ret_error, EINVAL);
    }
    return -sd_bus_error_get_errno(ret_error);
}

static map_ret_code check_client_inot(void *userdata, void *client) {
    int *num_idles = (int *)userdata;
    idle_client_t *c = (idle_client_t *)client;
    
    *num_idles += c->is_idle;
    if (*num_idles > 1) {
        return MAP_FULL;
    }
    return MAP_OK;
}

static int method_stop_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    idle_client_t *c = validate_client(sd_bus_message_get_path(m), m, ret_error);
    if (c) {
        /* You can only stop running clients */
        if (c->running) {
            /* Do not reset timerfd is client is in idle state */
            if (!c->is_idle) {
                struct itimerspec timerValue = {{0}};
                timerfd_settime(c->fd, 0, &timerValue, NULL);
            } else {
                /* 
                 * If this was the only client awaiting on inotify,
                 * remove inotify inotify_add_watcher
                 */
                int num_idles = 0;
                if (map_iterate(clients, check_client_inot, &num_idles) == MAP_OK) {
                    /* this is the only idle client */
                    m_log("Removing inotify watch as only client using it was stopped.\n");
                    inotify_rm_watch(inot_fd, inot_wd);
                    inot_wd = -1;
                }
            }
            /* Reset client state */
            c->running = false;
            c->is_idle = false;
            m_log("Stopping Client %u\n", c->id);
            return sd_bus_reply_method_return(m, NULL);
        }
        sd_bus_error_set_errno(ret_error, EINVAL);
    }
    return -sd_bus_error_get_errno(ret_error);
}

static int set_timeout(sd_bus *b, const char *path, const char *interface, const char *property, 
                       sd_bus_message *value, void *userdata, sd_bus_error *error) {

    idle_client_t *c = validate_client(path, value, error);
    if (!c) {
        return -sd_bus_error_get_errno(error);
    }
    
    int old_timer  = *(int *)userdata;
    int r = sd_bus_message_read(value, "u", userdata);
    if (r < 0) {
        m_log("Failed to set timeout.\n");
        return r;
    }

    if (c->running && !c->is_idle) {
        struct itimerspec timerValue = {{0}};

        int new_timer = *(int *)userdata;
        timerfd_gettime(c->fd, &timerValue);
        int old_elapsed = old_timer - timerValue.it_value.tv_sec;
        int new_timeout = new_timer - old_elapsed;
        if (new_timeout <= 0) {
            timerValue.it_value.tv_nsec = 1;
            timerValue.it_value.tv_sec = 0;
            m_log("Starting now.\n");
        } else {
            timerValue.it_value.tv_sec = new_timeout;
            m_log("Next timer: %d\n", new_timeout);
        }
        r = timerfd_settime(c->fd, 0, &timerValue, NULL);
    }
    return r;
}

static int set_prop(sd_bus *b, const char *path, const char *interface, const char *property, 
                       sd_bus_message *value, void *userdata, sd_bus_error *error) {
    idle_client_t *c = validate_client(path, value, error);
    if (!c) {
        return -sd_bus_error_get_errno(error);
    }
    
    const char *val = NULL;
    int r = sd_bus_message_read(value, "s", &val);
    if (r < 0) {
        m_log("Failed to set property %s.\n", property);
    } else {
        char **prop = (char **)userdata;
        *prop = strdup(val);
    }
    return r;
}

#endif
