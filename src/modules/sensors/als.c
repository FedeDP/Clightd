#include <sensor.h>
#include <udev.h>

#define ALS_NAME            "Als"
#define ALS_ILL_MAX         100000 // Direct Sunlight
#define ALS_ILL_MIN         20 // Dark Room
#define ALS_INTERVAL        20 // ms
#define ALS_SUBSYSTEM       "iio"

SENSOR(ALS_NAME);

static void parse_settings(char *settings, int *min, int *max, int *interval);

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

static void parse_settings(char *settings, int *min, int *max, int *interval) {
    const char opts[] = { 'i', 'm', 'M' };
    int *vals[] = { interval, min, max };

    /* Default values */
    *min = ALS_ILL_MIN;
    *max = ALS_ILL_MAX;
    *interval = ALS_INTERVAL;

    if (settings && strlen(settings)) {
        char *token; 
        char *rest = settings; 

        while ((token = strtok_r(rest, ",", &rest))) {
            char opt;
            int val;

            if (sscanf(token, "%c=%d", &opt, &val) == 2) {
                bool found = false;
                for (int i = 0; i < SIZE(opts) && !found; i++) {
                    if (opts[i] == opt) {
                        *(vals[i]) = val;
                        found = true;
                    }
                }

                if (!found) {
                    fprintf(stderr, "Option %c not found.\n", opt);
                }
            } else {
                fprintf(stderr, "Expected a=b format.\n");
            }
        }
    }
    
    /* Sanity checks */
    if (*interval < 0 || *interval > 1000) {
        fprintf(stderr, "Wrong interval value. Resetting default.\n");
        *interval = ALS_INTERVAL;
    }
    if (*min < 1) {
        fprintf(stderr, "Wrong min value. Resetting default.\n");
        *min = ALS_ILL_MIN;
    }
    if (*max < 1) {
        fprintf(stderr, "Wrong max value. Resetting default.\n");
        *max = ALS_ILL_MAX;
    }
    if (*min > *max) {
        fprintf(stderr, "Wrong min/max values. Resetting defaults.\n");
        *min = ALS_ILL_MIN;
        *max = ALS_ILL_MAX;
    }
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    int min, max, interval;
    parse_settings(settings, &min, &max, &interval);

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
                if (illuminance > max) {
                    illuminance = max;
                } else if (illuminance < min) {
                    illuminance = min;
                }
            }
        }

        if (illuminance >= 1) {
            if (illuminance != min) {
                pct[ctr++] = (log10(illuminance) - log10(min)) / log10(max);
            } else if (illuminance == min) {
                pct[ctr++] = 0.01;
            }
        }

        usleep(interval * 1000);
    }
    return ctr;
}
