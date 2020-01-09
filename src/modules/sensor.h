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
    int mon_handler;        // if an udev monitor is associated to this sensor, it will be != -1
    bool (*validate)(struct udev_device *dev);
    void (*fetch)(const char *subsystem, struct udev_device **dev);
    int (*capture)(struct udev_device *userdata, double *pct, const int num_captures, char *settings);
    char obj_path[100];
} sensor_t;

#define SENSOR(name, subsystem) \
    static bool validate(struct udev_device *dev); \
    static void fetch(const char *interface, struct udev_device **dev); \
    static int capture(struct udev_device *dev, double *pct, const int num_captures, char *settings); \
    static void _ctor_ register_sensor(void) { \
        const sensor_t self = { name, subsystem, -1, validate, fetch, capture }; \
        sensor_register_new(&self); \
    }

void sensor_register_new(const sensor_t *sensor);
