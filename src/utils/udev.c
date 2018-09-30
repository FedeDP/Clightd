#include <udev.h>

static void get_first_matching_device(struct udev_device **dev, const char *subsystem, const char *sysname);

static struct udev_monitor *mon;

int init_udev_monitor(const char *subsystem) {
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, subsystem, NULL);
    udev_monitor_enable_receiving(mon);
    return udev_monitor_get_fd(mon);
}

void receive_udev_device(struct udev_device **dev) {
    *dev = udev_monitor_receive_device(mon);
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
