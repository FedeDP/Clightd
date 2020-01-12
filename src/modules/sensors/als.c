#include <sensor.h>

#define ALS_NAME            "Als"
#define ALS_ILL_MAX         500
#define ALS_ILL_MIN         22
#define ALS_INTERVAL        20 // ms
#define ALS_SUBSYSTEM       "iio"

SENSOR(ALS_NAME, ALS_SUBSYSTEM);

static void parse_settings(char *settings, int *min, int *max, int *interval);

/* properties names to be checked. "in_illuminance_input" has higher priority. */
static const char *ill_names[] = { "in_illuminance_input", "in_illuminance_raw", "in_intensity_clear_raw" };
static const char *scale_names[] = { "in_illuminance_scale", "in_intensity_scale" };

static bool validate(struct udev_device *dev) {
    /* Check if any device exposes requested sysattr */
    for (int i = 0; i < SIZE(ill_names); i++) {
        if (udev_device_get_sysattr_value(dev, ill_names[i])) {
            return true;
        }
    }
    return false;
}

static void fetch(const char *interface, struct udev_device **dev) {
    /* Check if any device exposes requested sysattr */
    for (int i = 0; i < SIZE(ill_names) && !*dev; i++) {
        get_udev_device(interface, ALS_SUBSYSTEM, ill_names[i], NULL, dev);
    }
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
    if (*min < 0) {
        fprintf(stderr, "Wrong min value. Resetting default.\n");
        *min = ALS_ILL_MIN;
    }
    if (*max < 0) {
        fprintf(stderr, "Wrong max value. Resetting default.\n");
        *max = ALS_ILL_MAX;
    }
    if (*min > *max) {
        fprintf(stderr, "Wrong min/max values. Resetting defaults.\n");
        *min = ALS_ILL_MIN;
        *max = ALS_ILL_MAX;
    }
}

static int capture(struct udev_device *dev, double *pct, const int num_captures, char *settings) {
    int min, max, interval;
    parse_settings(settings, &min, &max, &interval);

    int ret = -num_captures;
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

        if (illuminance >= 0) {
            pct[i] = illuminance / max;
            ret++;
        }

        usleep(interval * 1000);
    }
    return ret; // 0 if all requested captures are fullfilled
}
