#include <udev.h>

static void get_first_matching_device(struct udev_device **dev, const char *subsystem, const udev_match *match);

int init_udev_monitor(const char *subsystem, struct udev_monitor **mon) {
    *mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(*mon, subsystem, NULL);
    udev_monitor_enable_receiving(*mon);
    return udev_monitor_get_fd(*mon);
}

/**
* Set dev to first device in subsystem, eventually matching requested sysattr existence.
*/
static void get_first_matching_device(struct udev_device **dev, const char *subsystem, 
                                      const udev_match *match) {
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, subsystem);
    if (match) {
        udev_enumerate_add_match_sysattr(enumerate, match->sysattr_key, match->sysattr_val);
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

void get_udev_device(const char *interface, const char *subsystem, const udev_match *match,
                            sd_bus_error **ret_error, struct udev_device **dev) {
    *dev = NULL;
    /* if no interface is specified, try to get first matching device */
    if (interface == NULL || interface[0] == '\0') {
        get_first_matching_device(dev, subsystem, match);
    } else {
        char *name = strrchr(interface, '/');
        if (name) {
            return get_udev_device(++name, subsystem, match, ret_error, dev);
        }
        *dev = udev_device_new_from_subsystem_sysname(udev, subsystem, interface);
    }
    if (!(*dev) && ret_error) {
        sd_bus_error_set_errno(*ret_error, ENODEV);
    }
}

void udev_devices_foreach(const char *subsystem, const udev_match *match,  
                          int (*cb)(struct udev_device *dev, void *userdata), void *userdata) {

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, subsystem);
    if (match) {
        if (match->sysattr_key) {
            udev_enumerate_add_match_sysattr(enumerate, match->sysattr_key, match->sysattr_val);
        }
        if (match->sysname) {
            udev_enumerate_add_match_sysname(enumerate, match->sysname);
        }
    }
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    if (devices) {
        struct udev_list_entry *entry = NULL;
        udev_list_entry_foreach(entry, devices) {
            const char *path = udev_list_entry_get_name(entry);
            struct udev_device *dev = udev_device_new_from_syspath(udev, path);
            int ret = cb(dev, userdata);
            udev_device_unref(dev);
            if (ret < 0) {
                break;
            }
        }
    }
     /* Free the enumerator object */
    udev_enumerate_unref(enumerate);
}
