#include <sensor.h>
#include <udev.h>
#include <iio.h>
#include <sys/time.h>
#include "als.h"

#define ALS_NAME            "Als"
#define ALS_SUBSYSTEM       "iio"
#define PROCESS_CHANNEL_BITS(bits)  val = ((int##bits##_t*)p_dat)[i];

// Buffer method has higher prio. See https://gitlab.freedesktop.org/hadess/iio-sensor-proxy/-/merge_requests/352.
typedef enum { ALS_IIO_BUFFER, ALS_IIO_POLL, ALS_IIO_MAX } als_iio_types;

typedef struct als_device {
    struct udev_device *dev;
    const char *attr_name[ALS_IIO_MAX];
    double (*capture[ALS_IIO_MAX])(struct als_device *als,  double *pct, const int num_captures, int interval);
} als_device_t;

SENSOR(ALS_NAME);

/* properties names to be checked. "in_illuminance_input" has higher priority. */
static const char *ill_poll_names[] = { "in_illuminance_input", "in_illuminance0_input", "in_illuminance_raw", "in_intensity_clear_raw" };
static const char *ill_buff_names[] = { "scan_elements/in_illuminance_en", "scan_elements/in_intensity_both_en" };
static const char *scale_names[] = { "in_illuminance_scale", "in_intensity_scale" };

static struct udev_monitor *mon;

static double iio_poll_capture(struct als_device *als, double *pct, const int num_captures, int interval) {
    const char *syspath = udev_device_get_syspath(als->dev);
    
    INFO("[IIO-POLL] Start capture: '%s' syspath.\n", syspath);
    
    // Load scale value
    const char *val = NULL;
    double scale = 1.0; // defaults to 1.0
    for (int i = 0; i < SIZE(scale_names) && !val; i++) {
        val = udev_device_get_sysattr_value(als->dev, scale_names[i]);
        if (val) {
            scale = atof(val);
        }
    }
    INFO("[IIO-POLL] Loaded scale: %f.\n", scale);
    
    
    int ctr = 0;
    for (int i = 0; i < num_captures; i++) {
        struct udev_device *non_cached_dev = udev_device_new_from_syspath(udev, syspath);
        val = udev_device_get_sysattr_value(non_cached_dev, als->attr_name[ALS_IIO_POLL]);
        INFO("[IIO-POLL] Read: %s.\n", val);
        if (val) {
            double illuminance = atof(val) * scale;
            ctr++;
            pct[i] = compute_value(illuminance);
            INFO("[IIO-POLL] Pct[%d] = %lf\n", i, pct[i]);
        }
        udev_device_unref(non_cached_dev);
        usleep(interval * 1000);
    }
    return ctr;
}

static double iio_buffer_capture(struct als_device *als, double *pct, const int num_captures, int interval) {
    int ctr = 0;
        
    const char *sysname = udev_device_get_sysname(als->dev);
    INFO("[IIO-BUF] Start capture: '%s' sysname.\n", sysname);
    
    
    /* Getting local iio device context */
    struct iio_context *local_ctx = iio_create_local_context();
    if (!local_ctx) {
        fprintf(stderr, "Failed to create local iio ctx.\n");
        return ctr;
    }
    
    const struct iio_device *dev = iio_context_find_device(local_ctx, sysname);
    if (!dev) {
        fprintf(stderr, "Couldn't find device '%s': %m\n", sysname);
        return ctr;
    }
    
    INFO("[IIO-BUF] Found device.\n");
    
    // Compute channel name from attribute ("illuminance" or "intensity_both")
    char *name = strrchr(als->attr_name[ALS_IIO_BUFFER], '/') + 1; // "scan_elements/in_illuminance_en" -> "in_illuminance_en"
    name = strchr(name, '_') + 1; // "in_illuminance_en" -> "illuminance_en"
    char *ptr = strrchr(name, '_');
    char channel_name[64];
    snprintf(channel_name, strlen(name) - strlen(ptr) + 1, "%s", name); // "illuminance_en" -> "illuminance"
    
    INFO("[IIO-BUF] Channel name: '%s'.\n", channel_name);
    
    struct iio_channel *ch = iio_device_find_channel(dev, channel_name, false);
    if (!ch) {
        fprintf(stderr, "Failed to fetch '%s' channel: %m\n", channel_name);
        return ctr;
    }
    
    if (iio_channel_is_output(ch)) {
        fprintf(stderr, "Wrong output channel selected.\n");
        return ctr;
    }
    if (!iio_channel_is_scan_element(ch)) {
        fprintf(stderr, "Channel is not a scan element.\n");
        return ctr;
    }
    
    iio_channel_enable(ch);
    if (!iio_channel_is_enabled(ch)) {
        fprintf(stderr, "Failed to enable channel '%s'!\n", channel_name);
        return ctr;
    }
    
    INFO("[IIO-BUF] Creating buffer.\n");
    struct iio_buffer *rxbuf = iio_device_create_buffer(dev, 1, false);
    if (!rxbuf) {
        fprintf(stderr, "Failed to allocated buffer: %m\n");
        return ctr;
    }
    
    // Load scale
    double scale = 1.0; // default to 1.0
    const struct iio_data_format *fmt = iio_channel_get_data_format(ch);
    if (!fmt) {
        fprintf(stderr, "Failed to fetch channel format.\n");
        return ctr;
    }
    if (fmt->with_scale) {
        scale = fmt->scale;
    }
    
    INFO("[IIO-BUF] Data fmt: bits: %d | signed: %d | len: %d | rep: %d | scale: %f | has_scale: %d | shift: %d.\n", 
           fmt->bits, fmt->is_signed, fmt->length, fmt->repeat, fmt->scale, fmt->with_scale, fmt->shift);

    const size_t read_size = fmt->bits / 8;
    for (int i = 0; i < num_captures; i++) {
        int ret = iio_buffer_refill(rxbuf);
        INFO("[IIO-BUF] Refill ret: %d/%ld\n", ret, read_size);
        if (ret == read_size) {
            int64_t val = 0;
            if (ret == read_size) {
                iio_channel_read(ch, rxbuf, &val, read_size);
                INFO("[IIO-BUF] Read %ld\n", val);
                double illuminance = (double)val * scale;
                ctr++;
                pct[i] = compute_value(illuminance);
                INFO("[IIO-BUF] Pct[%d] = %lf\n", i, pct[i]);
            }
        }
        usleep(interval * 1000);
    }
    iio_buffer_destroy(rxbuf);
    iio_context_destroy(local_ctx);
    return ctr;
}

static bool validate_dev(void *dev) {
    als_device_t *als = (als_device_t *)dev;
    
    bool valid = false;
    /* Check if device exposes any of the requested sysattrs */
    for (int i = 0; i < SIZE(ill_buff_names); i++) {
        if (udev_device_get_sysattr_value(als->dev, ill_buff_names[i])) {
            als->attr_name[ALS_IIO_BUFFER] = ill_buff_names[i];
            als->capture[ALS_IIO_BUFFER] = iio_buffer_capture;
            valid = true;
            INFO("Buffer available, using '%s' sysattr\n", ill_buff_names[i]);
            break;
        }
    }
    
    for (int i = 0; i < SIZE(ill_poll_names); i++) {
        if (udev_device_get_sysattr_value(als->dev, ill_poll_names[i])) {
            als->attr_name[ALS_IIO_POLL] = ill_poll_names[i];
            als->capture[ALS_IIO_POLL] = iio_poll_capture;
            valid = true;
            INFO("Poll available, using '%s' sysattr\n", ill_poll_names[i]);
            break;
        }
    }
    return valid;
}

static void fetch_dev(const char *interface, void **dev) {
    struct udev_device *d = NULL;
    /* Check if any device exposes requested sysattr */
    for (int i = 0; i < SIZE(ill_buff_names) && !d; i++) {
        /* Only check existence for needed sysattr */
        const udev_match match = { .sysattr_key = ill_buff_names[i] };
        get_udev_device(interface, ALS_SUBSYSTEM, &match, NULL, &d);
    }
    
    if (!d) {
        for (int i = 0; i < SIZE(ill_poll_names) && !d; i++) {
            /* Only check existence for needed sysattr */
            const udev_match match = { .sysattr_key = ill_poll_names[i] };
            get_udev_device(interface, ALS_SUBSYSTEM, &match, NULL, &d);
        }
    }
    
    if (d) {
        als_device_t *als = calloc(1, sizeof(als_device_t));
        als->dev = d;
        *dev = als;
    }
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    als_device_t *als = (als_device_t *)dev;
    if (node) {
        *node =  udev_device_get_devnode(als->dev);
    }
    if (action) {
        *action = udev_device_get_action(als->dev);
    }
}

static void destroy_dev(void *dev) {
    als_device_t *als = (als_device_t *)dev;
    udev_device_unref(als->dev);
    free(als);
}

static int init_monitor(void) {
    return init_udev_monitor(ALS_SUBSYSTEM, &mon);
}

static void recv_monitor(void **dev) {
    struct udev_device *d = udev_monitor_receive_device(mon);
    als_device_t *als = calloc(1, sizeof(als_device_t));
    als->dev = d;
    *dev = als;
}

static void destroy_monitor(void) {
    udev_monitor_unref(mon);
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    int interval;
    parse_settings(settings, &interval);

    als_device_t *als = (als_device_t *)dev;
    int ret = 0;
    // Try buffer then poll methods, if both are available
    for (int i = 0; i < ALS_IIO_MAX && ret == 0; i++) {
        if (als->capture[i]) {
            ret = als->capture[i](als, pct, num_captures, interval);
        }
    }
    return ret;
}
