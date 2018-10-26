#include <udev.h>

static void get_first_matching_device(struct udev_device **dev, const char *subsystem, const char *sysname);

static struct udev_monitor **mons;
static int num_monitor;

int init_udev_monitor(const char *subsystem, int *handler) {
    struct udev_monitor **tmp = realloc(mons, sizeof(struct udev_monitor *) * ++num_monitor);
    if (tmp) {
        mons = tmp;
        *handler = num_monitor - 1;
        mons[num_monitor - 1] = udev_monitor_new_from_netlink(udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(mons[num_monitor - 1], subsystem, NULL);
        udev_monitor_enable_receiving(mons[num_monitor - 1]);
        return udev_monitor_get_fd(mons[num_monitor - 1]);
    }
    return -1;
}

void receive_udev_device(struct udev_device **dev, int handler) {
    if (handler != -1) {
        *dev = udev_monitor_receive_device(mons[handler]);
    } else {
        *dev = NULL;
    }
}

void destroy_udev_monitors(void) {
    for (int i = 0; i < num_monitor; i++) {
        udev_monitor_unref(mons[i]);
    }
    free(mons);
    num_monitor = 0;
}

/**
 * Set dev to first device in subsystem
 */
static void get_first_matching_device(struct udev_device **dev, const char *subsystem, 
                                      const char *sysname) {
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, subsystem);
    if (sysname) {
        udev_enumerate_add_match_sysattr(enumerate, "name", sysname);
    }
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    if (devices) {
        const char *path = udev_list_entry_get_name(devices);
        *dev = udev_device_new_from_syspath(udev, path);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);
}

void get_udev_device(const char *interface, const char *subsystem, const char *sysname,
                            sd_bus_error **ret_error, struct udev_device **dev) {
    *dev = NULL;
    /* if no interface is specified, try to get first matching device */
    if (!interface || !strlen(interface)) {
        get_first_matching_device(dev, subsystem, sysname);
    } else {
        char *name = strrchr(interface, '/');
        if (name) {
            return get_udev_device(++name, subsystem, sysname, ret_error, dev);
        }
        *dev = udev_device_new_from_subsystem_sysname(udev, subsystem, interface);
    }
    if (!(*dev) && ret_error) {
        sd_bus_error_set_errno(*ret_error, ENODEV);
    }
}
