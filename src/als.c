#include "../inc/als.h"
#include "../inc/udev.h"
#include "../inc/polkit.h"

#define ILL_MAX         4096
#define ALS_SUBSYSTEM   "iio"

int method_isalsavailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct udev_device *dev = NULL;
    int present = 0;
    
    get_udev_device(NULL, ALS_SUBSYSTEM, NULL, &dev);
    if (dev) {
        present = 1;
        udev_device_unref(dev);
    }
    return sd_bus_reply_method_return(m, "b", present);
}

/*
 * Frame capturing method
 */
int method_captureals(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r;
    struct udev_device *dev = NULL;
    const char *interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(interface, ALS_SUBSYSTEM, &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    int32_t illuminance = atoi(udev_device_get_sysattr_value(dev, "in_illuminance_input"));
    double pct = illuminance / ILL_MAX;
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "d", pct);
}
