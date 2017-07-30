#include "../inc/backlight.h"
#include "../inc/polkit.h"
#include "../inc/udev.h"

/**
 * Brightness setter method
 */
int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int value, r, max;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "si", &backlight_interface, &value);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* Return an error if value is < 0 */
    if (value < 0) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Value must be greater or equal to 0.");
        return -EINVAL;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    /**
     * Check if value is <= max_brightness value
     */
    max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    if (value > max) {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Value must be smaller than %d.", max);
        return -EINVAL;
    }
    
    char val[10];
    sprintf(val, "%d", value);
    r = udev_device_set_sysattr_value(dev, "brightness", val);
    if (r < 0) {
        udev_device_unref(dev);
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_ACCESS_DENIED, "Not authorized.");
        return r;
    }
    
    printf("New brightness value for %s: %d\n", udev_device_get_sysname(dev), value);
    
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "i", value);
}

/**
 * Current brightness getter method
 */
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    x = atoi(udev_device_get_sysattr_value(dev, "brightness"));
    printf("Current brightness value for %s: %d\n", udev_device_get_sysname(dev), x);
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

/**
 * Max brightness value getter method
 */
int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    x = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    printf("Max brightness value for %s: %d\n", udev_device_get_sysname(dev), x);
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

/**
 * Actual brightness value getter method
 */
int method_getactualbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    x = atoi(udev_device_get_sysattr_value(dev, "actual_brightness"));
    printf("Actual brightness value for %s: %d\n", udev_device_get_sysname(dev), x);
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

int method_isinterface_enabled(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r, e = 1; // default to enabled if sysattr is missing
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    const char *sysname = udev_device_get_sysname(dev);
    dev = udev_device_get_parent_with_subsystem_devtype(dev, "drm", NULL);
    if (!dev) {
        sd_bus_error_set_errno(ret_error, ENODEV);
        return -sd_bus_error_get_errno(ret_error);
    }
    
    const char *enabled = udev_device_get_sysattr_value(dev, "enabled");
    if (enabled) {
        printf("Interface %s state: %s\n", sysname, enabled);
        e = !strcmp(enabled, "enabled"); // 1 if enabled
    }
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "b", e);
    
}
