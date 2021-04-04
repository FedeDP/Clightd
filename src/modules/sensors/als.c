#include <sensor.h>
#include <udev.h>
#include "als.h"

#define ALS_NAME            "Als"
#define ALS_SUBSYSTEM       "iio"

SENSOR(ALS_NAME);

/* properties names to be checked. "in_illuminance_input" has higher priority. */
static const char *ill_names[] = { "in_illuminance_input", "in_illuminance_raw", "in_intensity_clear_raw" };
static const char *scale_names[] = { "in_illuminance_scale", "in_intensity_scale" };

static struct udev_monitor *mon;

static bool validate_dev(void *dev) {
    /* Check if device exposes any of the requested sysattrs */
    for (int i = 0; i < SIZE(ill_names); i++) {
        if (udev_device_get_sysattr_value(dev, ill_names[i])) {
            return true;
        }
    }
    return false;
}

static void fetch_dev(const char *interface, void **dev) {
    /* Check if any device exposes requested sysattr */
    for (int i = 0; i < SIZE(ill_names) && !*dev; i++) {
        /* Only check existence for needed sysattr */
        const udev_match match = { ill_names[i] };
        get_udev_device(interface, ALS_SUBSYSTEM, &match, NULL, (struct udev_device **)dev);
    }
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    if (node) {
        *node =  udev_device_get_devnode(dev);
    }
    if (action) {
        *action = udev_device_get_action(dev);
    }
}

static void destroy_dev(void *dev) {
    udev_device_unref(dev);
}

static int init_monitor(void) {
    return init_udev_monitor(ALS_SUBSYSTEM, &mon);
}

static void recv_monitor(void **dev) {
    *dev = udev_monitor_receive_device(mon);
}

static void destroy_monitor(void) {
    udev_monitor_unref(mon);
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    int interval;
    parse_settings(settings, &interval);

    int ctr = 0;
    const char *val = NULL;

    /* Properly load scale value; defaults to 1.0 */
    double scale = 1.0;
    for (int i = 0; i < SIZE(scale_names) && !val; i++) {
        val = udev_device_get_sysattr_value(dev, scale_names[i]);
        if (val) {
            scale = atof(val);
        }
    }

    for (int i = 0; i < num_captures; i++) {
        double illuminance = -1;
        for (int i = 0; i < SIZE(ill_names) && illuminance == -1; i++) {
            val = udev_device_get_sysattr_value(dev, ill_names[i]);
            if (val) {
                illuminance = atof(val) * scale;
            }
        }

        if (illuminance >= 1) {
            ctr++;
            pct[i] = compute_value(illuminance);
        }

        usleep(interval * 1000);
    }
    return ctr;
}
