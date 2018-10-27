#include <modules.h>

static int get_version( sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);

static const char object_path[] = "/org/clightd/clightd";
static const char bus_interface[] = "org.clightd.clightd";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END
};

sd_bus *bus;

MODULE(BUS);

static int init(void) {
    int r = sd_bus_default_system(&bus);
    if (r < 0) {
        MODULE_ERR("Failed to connect to system bus: %s\n", strerror(-r));
    } else {
        /* Install the object */
        r = sd_bus_add_object_vtable(bus,
                                 NULL,
                                 object_path,
                                 bus_interface,
                                 vtable,
                                 NULL);
        if (r < 0) {
            MODULE_ERR("Failed to issue method call: %s\n", strerror(-r));
        }
    }
    /* Process initial messages */
    callback(GET_FD());
    return REGISTER_FD(sd_bus_get_fd(bus));
}

static int callback(const int fd) {
    int r;
    do {
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            MODULE_ERR("Failed to process bus: %s\n", strerror(-r));
            return LEAVE_W_ERR;
        }
    } while (r > 0);
    return 0;
}

static void destroy(void) {
    if (bus) {
        sd_bus_flush_close_unref(bus);
    }
}

static int get_version( sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", VERSION);
}
