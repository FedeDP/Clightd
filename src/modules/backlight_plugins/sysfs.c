#include "backlight.h"
#include <udev.h>

#define BL_SUBSYSTEM        "backlight"

static int store_internal_device(struct udev_device *dev, void *userdata);

BACKLIGHT("Sysfs");

static struct udev_monitor *bl_mon;

static void load_env(void) {
    return;
}

static void load_devices(void) {
    udev_devices_foreach(BL_SUBSYSTEM, NULL, store_internal_device, NULL);
}

static int get_monitor(void) {
    return init_udev_monitor(BL_SUBSYSTEM, &bl_mon);
}

static void receive(void) {
    /* From udev monitor, consume! */
    struct udev_device *dev = udev_monitor_receive_device(bl_mon);
    if (dev) {
        // Ok, the event was from internal monitor
        const char *id = udev_device_get_sysname(dev);
        const char *action = udev_device_get_action(dev);
        if (action) {
            bl_t *bl = map_get(bls, id);
            if (!strcmp(action, UDEV_ACTION_CHANGE) && bl) {
                // Load cached value
                int old_bl_value = atoi(udev_device_get_sysattr_value(bl->dev, "brightness"));
                /* Keep our device ref in sync! */
                udev_device_unref(bl->dev);
                bl->dev = udev_device_ref(dev);
                
                int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
                if (val != old_bl_value) {
                    const double pct = (double)val / bl->max;
                    emit_signals(bl, pct);
                }
            } else if (!strcmp(action, UDEV_ACTION_ADD) && !bl) {
                store_internal_device(dev, NULL);
            } else if (!strcmp(action, UDEV_ACTION_RM) && bl) {
                map_remove(bls, id);
            }
        }
        udev_device_unref(dev);
    }
}

static int set(bl_t *dev, int value) {
    char val[15] = {0};
    snprintf(val, sizeof(val) - 1, "%d", value);
    return udev_device_set_sysattr_value(dev->dev, "brightness", val);
}

static int get(bl_t *dev) {
    return atoi(udev_device_get_sysattr_value(dev->dev, "brightness"));
}

static void free_device(bl_t *dev) {
    udev_device_unref(dev->dev);
}

static void dtor(void) {
    udev_monitor_unref(bl_mon);
}

static int store_internal_device(struct udev_device *dev, void *userdata) {
    int ret = -ENOMEM;
    const int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    const char *id = udev_device_get_sysname(dev);
    bl_t *d = calloc(1, sizeof(bl_t));
    if (d) {
        d->is_internal = true;
        d->dev = udev_device_ref(dev);
        d->max = max;
        d->sn = strdup(id);
        // Unused. But receive() callback expects brightness value to be cached
        udev_device_get_sysattr_value(dev, "brightness");
        ret = store_device(d, SYSFS);
    }
    return ret;
}
