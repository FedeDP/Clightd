#ifdef IDLE_PRESENT

#include <commons.h>
#include <module/module_easy.h>
#include <X11/extensions/scrnsaver.h>

typedef struct {
    bool in_use;
    const char *xauthority;
    const char *display;
    unsigned int timeout;
    unsigned int id;
    int idle_fd;
    char *sender;
    char path[256];
} idle_client_t;

static int find_available_client(idle_client_t **c);
static idle_client_t *find_client(const int id);
static int method_get_idle_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_get_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_rm_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_start_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_stop_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static time_t get_idle_time(const char *display, const char *xauthority);

static uint64_t num_clients;
static idle_client_t *clients;
static const char object_path[] = "/org/clightd/clightd/Idle";
static const char bus_interface[] = "org.clightd.clightd.Idle";
static const char clients_interface[] = "org.clightd.clightd.Idle.Client";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetClient", NULL, "o", method_get_client, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("DestroyClient", "o", NULL, method_rm_client, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable vtable_clients[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Start", NULL, NULL, method_start_client, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Stop", NULL, NULL, method_stop_client, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_WRITABLE_PROPERTY("Display", "s", NULL, NULL, offsetof(idle_client_t, display), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_WRITABLE_PROPERTY("Xauthority", "s", NULL, NULL, offsetof(idle_client_t, xauthority), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_WRITABLE_PROPERTY("Timeout", "u", NULL, NULL, offsetof(idle_client_t, timeout), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Idle", "ss", 0),
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
    if (!msg->is_pubsub) {
        uint64_t t;
        // nonblocking mode!
        read(msg->fd_msg->fd, &t, sizeof(uint64_t));
        
        idle_client_t *c = (idle_client_t *)msg->fd_msg->userptr;
        
        // Re-arm our timer
        struct itimerspec timerValue = {{0}};
        timerValue.it_value.tv_sec = c->timeout;
        timerfd_settime(msg->fd_msg->fd, 0, &timerValue, NULL);
        
        m_log("Received from client %d!\n", c->id);
        sd_bus_emit_signal(bus, c->path, clients_interface, "Elapsed", NULL);
    }
}

static void destroy(void) {
    free(clients);
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

static int method_get_idle_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
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

static int find_available_client(idle_client_t **c) {
    for (int i = 0; i < num_clients; i++) {
        if (!clients[i].in_use) {
            *c = &clients[i];
            return i;
        }
    }
    idle_client_t *tmp = realloc(clients, (num_clients + 1) * sizeof(idle_client_t));
    if (tmp) {
        clients = tmp;
        *c = &clients[num_clients++];
        memset(*c, 0, sizeof(idle_client_t));
        return num_clients - 1;
    }
    return -1;
}

static idle_client_t *find_client(const int id) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].in_use && clients[i].id == id) {
            return &clients[i];
        }
    }
    return NULL;
}

static idle_client_t *validate_client(const char *path, sd_bus_message *m, sd_bus_error *ret_error) {
    int id;
    if (sscanf(path, "/org/clightd/clightd/Idle/Client%d", &id) == 1) {
        idle_client_t *c = find_client(id);
        // FIXME: decomment before release!
        if (c && c->in_use /*&& !strcmp(c->sender, sd_bus_message_get_sender(m))*/) {
            return c;
        }
    }
    m_log("Failed to validate client.\n");
    sd_bus_error_set_errno(ret_error, EINVAL);
    return NULL;
}

static int method_get_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    idle_client_t *c = NULL;
    const int id = find_available_client(&c);
    if (id != -1) {        
        c->in_use = true;
        c->id = id;
        c->idle_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        m_register_fd(c->idle_fd, true, c);
        c->sender = strdup(sd_bus_message_get_sender(m));
        snprintf(c->path, sizeof(c->path), "%s/Client%d", object_path, c->id);
        sd_bus_add_object_vtable(bus,
                                NULL,
                                c->path,
                                clients_interface,
                                vtable_clients,
                                c);
        m_log("Created client %d\n", c->id);
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
        m_log("Freed client %d\n", c->id);
        m_deregister_fd(c->idle_fd);
        free(c->sender);
        memset(c, 0, sizeof(idle_client_t));
        return sd_bus_reply_method_return(m, NULL);
    }
    return -EINVAL;
}

static int method_start_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    idle_client_t *c = validate_client(sd_bus_message_get_path(m), m, ret_error);
    if (c) {
        struct itimerspec timerValue = {{0}};
        timerValue.it_value.tv_sec = c->timeout;
        timerfd_settime(c->idle_fd, 0, &timerValue, NULL);
        m_log("Starting Client %d\n", c->id);
        return sd_bus_reply_method_return(m, NULL);
    }
    return -EINVAL;
}

static int method_stop_client(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    idle_client_t *c = validate_client(sd_bus_message_get_path(m), m, ret_error);
    if (c) {
        struct itimerspec timerValue = {{0}};
        timerfd_settime(c->idle_fd, 0, &timerValue, NULL);
        m_log("Stopping Client %d\n", c->id);
        return sd_bus_reply_method_return(m, NULL);
    }
    return -EINVAL;
}

#endif
