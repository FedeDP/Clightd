#include "../inc/sensor.h"
#include "../inc/polkit.h"

static enum sensors get_sensor_type(const char *str);
static int is_sensor_available(sensor_t *sensor, const char *interface, 
                               sd_bus_error **ret_error, struct udev_device **device);

static sensor_t sensors[SENSOR_NUM];

void register_new_sensor(const sensor_t *sensor) {
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
                               sd_bus_error **ret_error, struct udev_device **device) {
    int present = 0;
    
    struct udev_device *dev = NULL;
    get_udev_device(NULL, sensor->subsystem, sensor->udev_name, NULL, &dev);
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
    if (s < SENSOR_NUM) {
        present = is_sensor_available(&sensors[s], NULL, NULL, NULL);
    } else {
        for (int i = 0; i < SENSOR_NUM; i++) {
            present += is_sensor_available(&sensors[i], NULL, NULL, NULL);
        }
    }
    return sd_bus_reply_method_return(m, "b", present);
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
    const char *member = sd_bus_message_get_member(m);
    enum sensors s = get_sensor_type(member);
    if (s != SENSOR_NUM) {
        if (is_sensor_available(&sensors[s], interface, &ret_error, &dev)) {
            /* Bus Interface required sensor-specific method */
            r = sensors[s].capture_method(m, dev, ret_error);
        }
    } else {
        /* For CaptureSensor generic method, call capture_method on first available sensor */
        for (s = 0; s < SENSOR_NUM; s++) {
            if (is_sensor_available(&sensors[s], interface, &ret_error, &dev)) {
                r = sensors[s].capture_method(m, dev, ret_error);
                break;
            }
        }
    }
    
    /* Properly free dev if needed */
    if (dev) {
        udev_device_unref(dev);
    }
    
    /* No sensors available */
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    return r;
}
