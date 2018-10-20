#include <sensor.h>
#include <polkit.h>

static enum sensors get_sensor_type(const char *str);
static int is_sensor_available(sensor_t *sensor, const char *interface, 
                                struct udev_device **device);

static sensor_t sensors[SENSOR_NUM];

void sensor_register_new(const sensor_t *sensor) {
    const enum sensors s = get_sensor_type(sensor->name);
    if (s < SENSOR_NUM) {
        sensors[s] = *sensor;
    } else {
        fprintf(stderr, "Sensor not recognized. Not registering.\n");
    }
}

int sensor_get_monitor(const enum sensors s) {
    return init_udev_monitor(sensors[s].subsystem);
}

void sensor_receive_device(const enum sensors s, struct udev_device **dev) {
    struct udev_device *d = NULL;
    receive_udev_device(&d);
    if (d && (!sensors[s].udev_name || 
        !strcmp(udev_device_get_sysattr_value(d, "name"), sensors[s].udev_name))) {
         
        *dev = d;
    } else {
        *dev = NULL;
    }
}

static enum sensors get_sensor_type(const char *str) {
    static const char *sensor_names[] = {
    #define X(name, val) #name,
        _SENSORS
    #undef X
    };
    
    enum sensors s = SENSOR_NUM;
    for (int i = 0; i < SENSOR_NUM && s == SENSOR_NUM; i++) {
        if (strcasestr(str, sensor_names[i])) {
            s = i;
        }
    }
    return s;
}

static int is_sensor_available(sensor_t *sensor, const char *interface, 
                                struct udev_device **device) {
    int present = 0;
    
    struct udev_device *dev = NULL;
    get_udev_device(interface, sensor->subsystem, sensor->udev_name, NULL, &dev);
    if (dev) {
        present = 1;
        if (device) {
            *device = dev;
        } else {
            udev_device_unref(dev);
        }
    }
    return present;
}

int method_issensoravailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *member = sd_bus_message_get_member(m);
    const enum sensors s = get_sensor_type(member);
    
    int present = 0;
    const char *interface = NULL;
    int r = sd_bus_message_read(m, "s", &interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    struct udev_device *dev = NULL;
    if (s < SENSOR_NUM) {
        present = is_sensor_available(&sensors[s], interface, &dev);
    } else {
        for (int i = 0; i < SENSOR_NUM && !present; i++) {
            present = is_sensor_available(&sensors[i], interface, &dev);
        }
    }
    
    if (dev) {
        r = sd_bus_reply_method_return(m, "sb", udev_device_get_devnode(dev), present);
        udev_device_unref(dev);
    } else {
        r = sd_bus_reply_method_return(m, "sb", interface, present);
    }
    return r;
}

int method_capturesensor(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    const char *interface = NULL;
    int r = sd_bus_message_read(m, "s", &interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    struct udev_device *dev = NULL;
    double pct = -1.0;
    const char *member = sd_bus_message_get_member(m);
    enum sensors s = get_sensor_type(member);
    
    // default value
    r = -ENODEV;
    if (s != SENSOR_NUM) {
        if (is_sensor_available(&sensors[s], interface, &dev)) {
            /* Bus Interface required sensor-specific method */
            r = sensors[s].capture_method(dev, &pct);
        }
    } else {
        /* For CaptureSensor generic method, call capture_method on first available sensor */
        for (s = 0; s < SENSOR_NUM; s++) {
            if (is_sensor_available(&sensors[s], interface, &dev)) {
                r = sensors[s].capture_method(dev, &pct);
                break;
            }
        }
    }
    
    /* No sensors available */
    if (r < 0) {
        sd_bus_error_set_errno(ret_error, -r);
    } else {
        r = sd_bus_reply_method_return(m, "sd", udev_device_get_devnode(dev), pct);
    }
    
    /* Properly free dev if needed */
    if (dev) {
        udev_device_unref(dev);
    }
    
    return r;
}
