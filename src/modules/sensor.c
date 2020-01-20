#include <sensor.h>
#include <polkit.h>

#define SENSOR_MAX_CAPTURES    20

static bool is_sensor_available(sensor_t *sensor, const char *interface, 
                                void **device);
static void *find_available_sensor(sensor_t *sensor, const char *interface, void **dev);
static void sensor_receive_device(const sensor_t *sensor, void **dev);
static int method_issensoravailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_capturesensor(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sensor_t *sensors[SENSOR_NUM];
static const char object_path[] = "/org/clightd/clightd/Sensor";
static const char bus_interface[] = "org.clightd.clightd.Sensor";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Capture", "sis", "sad", method_capturesensor, SD_BUS_VTABLE_UNPRIVILEGED),
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
    for (int i = 0; i < SENSOR_NUM && !r; i++) {
        snprintf(sensors[i]->obj_path, sizeof(sensors[i]->obj_path) - 1, "%s/%s", object_path, sensors[i]->name);
        r += sd_bus_add_object_vtable(bus,
                                    NULL,
                                    sensors[i]->obj_path,
                                    bus_interface,
                                    vtable,
                                    sensors[i]);
        r += m_register_fd(sensors[i]->init_monitor(), false, sensors[i]);
    }
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        sensor_t *sensor = (sensor_t *)msg->fd_msg->userptr;
        void *dev = NULL;
        sensor_receive_device(sensor, &dev);
        if (dev) {
            const char *node = NULL;
            const char *action = NULL;
            sensor->fetch_props_dev(dev, &node, &action);
            
            sd_bus_emit_signal(bus, sensor->obj_path, bus_interface, "Changed", "ss", node, action);
            /* Changed is emitted on Sensor main object too */
            sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "ss", node, action);
            sensor->destroy_dev(dev);
        }
    }
}

static void destroy(void) {
    for (int i = 0; i < SENSOR_NUM; i++) {
        sensors[i]->destroy_monitor();
    }
}

void sensor_register_new(sensor_t *sensor) {
    const char *sensor_names[] = {
    #define X(name, val) #name,
        _SENSORS
    #undef X
    };
    
    enum sensors s;
    for (s = 0; s < SENSOR_NUM; s++) {
        if (strcasestr(sensor->name, sensor_names[s])) {
            break;
        }
    }
    
    if (s < SENSOR_NUM) {
        sensors[s] = sensor;
        printf("Registered %s sensor.\n", sensor->name);
    } else {
        printf("Sensor not recognized. Not registering.\n");
    }
}

static void sensor_receive_device(const sensor_t *sensor, void **dev) {
    *dev = NULL;
    if (sensor) {
        void *d = NULL;
        sensor->recv_monitor(&d);
        if (d && sensor->validate_dev(d)) {
            *dev = d;
        } else if (d) {
            sensor->destroy_dev(d);
        }
    }
}

static bool is_sensor_available(sensor_t *sensor, const char *interface, 
                                void **device) {    
    sensor->fetch_dev(interface, device);
    return *device != NULL;
}

static void *find_available_sensor(sensor_t *sensor, const char *interface, void **dev) {
    *dev = NULL;
    
    if (!sensor) {
        /* No sensor requested; check first available one */
        for (enum sensors s = 0; s < SENSOR_NUM && !*dev; s++) {
            is_sensor_available(sensors[s], interface, dev);
        }
    } else {
        is_sensor_available(sensor, interface, dev);
    }
    
    if (*dev) {
        return sensor;
    }
    return NULL;
}

static int method_issensoravailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *interface = NULL;
    int r = sd_bus_message_read(m, "s", &interface);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    void *dev = NULL;
    sensor_t *sensor = find_available_sensor(userdata, interface, &dev);
    if (dev) {
        const char *node = NULL;
        sensor->fetch_props_dev(dev, &node, NULL);
        r = sd_bus_reply_method_return(m, "sb", node, true);
        sensor->destroy_dev(dev);
    } else {
        r = sd_bus_reply_method_return(m, "sb", interface, false);
    }
    return r;
}

static int method_capturesensor(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    const char *interface = NULL;
    char *settings = NULL;
    const int num_captures;
    int r = sd_bus_message_read(m, "sis", &interface, &num_captures, &settings);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (num_captures <= 0 || num_captures > SENSOR_MAX_CAPTURES) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Number of captures should be between 1 and 20.");
        return -EINVAL;
    }
    
    void *dev = NULL;
    sensor_t *sensor = NULL;
    double *pct = calloc(num_captures, sizeof(double));
    if (pct) {
        sensor = find_available_sensor(userdata, interface, &dev);
    
        // default value
        r = -ENODEV;
        if (sensor) {
            /* Bus Interface required sensor-specific method */
            r = sensor->capture(dev, pct, num_captures, settings);
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
        
        const char *node = NULL;
        sensor->fetch_props_dev(dev, &node, NULL);
        sd_bus_message_append(reply, "s", node);
        sd_bus_message_append_array(reply, 'd', pct, num_captures * sizeof(double));
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_message_unref(reply);
    }
    
    /* Properly free dev if needed */
    if (sensor) {
        sensor->destroy_dev(dev);
    }
    free(pct);
    return r;
}
