#include <commons.h>
#include <sensor.h>
#include <polkit.h>

static enum sensors get_sensor_type(const char *str);
static int is_sensor_available(sensor_t *sensor, const char *interface, 
                                struct udev_device **device);
static int sensor_get_monitor(const enum sensors s);
static void sensor_receive_device(const sensor_t *sensor, struct udev_device **dev);
static int method_issensoravailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_capturesensor(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sensor_t sensors[SENSOR_NUM];
static const char object_path[] = "/org/clightd/clightd/Sensor";
static const char bus_interface[] = "org.clightd.clightd.Sensor";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Capture", "si", "sad", method_capturesensor, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("IsAvailable", "s", "sb", method_issensoravailable, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "ss", 0),
    SD_BUS_VTABLE_END
};

MODULE("SENSOR");

static void module_pre_start(void) {
    
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    int r = sd_bus_add_object_vtable(bus,
                                     NULL,
                                     object_path,
                                     bus_interface,
                                     vtable,
                                     NULL);
    for (int i = ALS; i < SENSOR_NUM && !r; i++) {
        snprintf(sensors[i].obj_path, sizeof(sensors[i].obj_path) - 1, "%s/%s", object_path, sensors[i].name);
        r += sd_bus_add_object_vtable(bus,
                                     NULL,
                                     sensors[i].obj_path,
                                     bus_interface,
                                     vtable,
                                     NULL);
        r += m_register_fd(sensor_get_monitor(i), false, &sensors[i]);
    }
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        sensor_t *s = (sensor_t *)msg->fd_msg->userptr;
        struct udev_device *dev = NULL;
        sensor_receive_device(s, &dev);
        if (dev) {
            sd_bus_emit_signal(bus, s->obj_path, bus_interface, "Changed", "ss", udev_device_get_devnode(dev), udev_device_get_action(dev));
            /* Changed is emitted on Sensor object too */
            sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "ss", udev_device_get_devnode(dev), udev_device_get_action(dev));
            udev_device_unref(dev);
        }
    }
}

static void destroy(void) {
    destroy_udev_monitors();
}

void sensor_register_new(const sensor_t *sensor) {
    const enum sensors s = get_sensor_type(sensor->name);
    if (s < SENSOR_NUM) {
        sensors[s] = *sensor;
        printf("Registered %s sensor.\n", sensor->name);
    } else {
        printf("Sensor not recognized. Not registering.\n");
    }
}

static int sensor_get_monitor(const enum sensors s) {
    return init_udev_monitor(sensors[s].subsystem, &sensors[s].mon_handler);
}

static void sensor_receive_device(const sensor_t *sensor, struct udev_device **dev) {
    *dev = NULL;
    if (sensor) {
        struct udev_device *d = NULL;
        receive_udev_device(&d, sensor->mon_handler);
        if (d && (!sensor->udev_name || 
            !strcmp(udev_device_get_sysattr_value(d, "name"), sensor->udev_name))) {

            *dev = d;
        } else if (d) {
            udev_device_unref(d);
        }
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

static int method_issensoravailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *member = sd_bus_message_get_path(m);
    const enum sensors s = get_sensor_type(member);
    
    int present = 0;
    const char *interface = NULL;
    int r = sd_bus_message_read(m, "s", &interface);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
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

static int method_capturesensor(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    const char *interface = NULL;
    const int num_captures;
    int r = sd_bus_message_read(m, "si", &interface, &num_captures);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (num_captures <= 0 || num_captures > 20) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Number of captures should be between 1 and 20.");
        return -EINVAL;
    }
    
    struct udev_device *dev = NULL;
    double *pct = calloc(num_captures, sizeof(double));
    if (pct) {
        const char *member = sd_bus_message_get_path(m);
        enum sensors s = get_sensor_type(member);
    
        // default value
        r = -ENODEV;
        if (s != SENSOR_NUM) {
            if (is_sensor_available(&sensors[s], interface, &dev)) {
                /* Bus Interface required sensor-specific method */
                r = sensors[s].capture_method(dev, pct, num_captures);
            }
        } else {
            /* For CaptureSensor generic method, call capture_method on first available sensor */
            for (s = 0; s < SENSOR_NUM; s++) {
                if (is_sensor_available(&sensors[s], interface, &dev)) {
                    r = sensors[s].capture_method(dev, pct, num_captures);
                    break;
                }
            }
        }
    } else {
        r = -ENOMEM;
    }
    
    /* No sensors available */
    if (r < 0) {
        sd_bus_error_set_errno(ret_error, -r);
    } else {
        /* Reply with array response */
        sd_bus_message *reply = NULL;
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_append(reply, "s", udev_device_get_devnode(dev));
        sd_bus_message_append_array(reply, 'd', pct, num_captures * sizeof(double));
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_message_unref(reply);
        
        m_log("%d frames captured by %s.\n", num_captures, udev_device_get_devnode(dev));
    }
    
    /* Properly free dev if needed */
    if (dev) {
        udev_device_unref(dev);
    }
    free(pct);
    return r;
}
