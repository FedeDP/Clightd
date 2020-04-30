#include <commons.h>

static int get_version( sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);

static const char object_path[] = "/org/clightd/clightd";
static const char bus_interface[] = "org.clightd.clightd";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END
};

MODULE("BUS");

static void module_pre_start(void) {
    sd_bus_default_system(&bus);
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
    } else {
        /* Process initial messages */
        receive(NULL, NULL);
        int fd = sd_bus_get_fd(bus);
        m_register_fd(dup(fd), true, NULL);
    }
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg || !msg->is_pubsub) {
        int r;
        do {
            r = sd_bus_process(bus, NULL);
            if (r < 0) {
                m_log("Failed to process bus: %s\n", strerror(-r));
                if (r == -ENOTCONN || r == -ECONNRESET) {
                    modules_quit(r);
                }
            }
        } while (r > 0);
    }
}

static void destroy(void) {
    sd_bus_flush_close_unref(bus);
}

static int get_version( sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", VERSION);
}
