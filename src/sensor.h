#include <udev.h>

#define _ctor_ __attribute__((constructor))

#define _SENSORS \
    X(ALS, 0) \
    X(WEBCAM, 1) \
    X(SENSOR_NUM, 2)

enum sensors { 
#define X(name, val) name = val,
    _SENSORS
#undef X
};

typedef struct _sensor {
    const char *name;       // actually sensor name -> note that exposed bus methods should have "name" in them
    const char *subsystem;  // udev subsystem
    const char *udev_name;  // required udev name (used by als sensor that REQUIRES "acpi-als" name, as "iio" subsystem alone is not enough to identify it)
    int (*capture_method)(struct udev_device *userdata, double *pct, const int num_captures);
} sensor_t;

#define SENSOR(type, subsystem, udev_name) \
    static int capture(struct udev_device *dev, double *pct, const int num_captures); \
    static void _ctor_ register_sensor(void) { \
        const sensor_t self = { type, subsystem, udev_name, capture }; \
        sensor_register_new(&self); \
    }
    
void sensor_register_new(const sensor_t *sensor);
int sensor_get_monitor(const enum sensors s);
void sensor_receive_device(const enum sensors s, struct udev_device **dev);
int method_issensoravailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_capturesensor(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
