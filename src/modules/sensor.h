#include <udev.h>

#define _ctor_     __attribute__((constructor (101))) // Used for Sensors registering

#define _SENSORS \
    X(ALS, 0) \
    X(CAMERA, 1) \
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
    int mon_handler;        // if an udev monitor is associated to this sensor, it will be != -1
    int (*capture_method)(struct udev_device *userdata, double *pct, const int num_captures);
    char obj_path[100];
} sensor_t;

#define SENSOR(type, subsystem, udev_name) \
    static int capture(struct udev_device *dev, double *pct, const int num_captures); \
    static void _ctor_ register_sensor(void) { \
        const sensor_t self = { type, subsystem, udev_name, -1, capture }; \
        sensor_register_new(&self); \
    }

void sensor_register_new(const sensor_t *sensor);
