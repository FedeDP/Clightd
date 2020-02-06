/**
 * Sensor Interface:
 * 
 * Any sensor should expose this interface:
 * - a name, that will be used to create sensor's dbus object path.
 * - a bunch of callbacks:
 * 
 * -> validate_dev() called when retrieving a device from a monitor to validate retrieved device.
 * -> fetch_dev() called by is_sensor_available(), to fetch a device (with requqested interface, if set)
 * -> fetch_props_dev() to retrieve a device properties, ie: its node and, if (action), an associated action
 * -> destroy_dev() to free dev resources
 * 
 * -> init_monitor() to initialize a monitor, returning a valid fd (or < 0 if useless)
 * -> recv_monitor() to retrieve a device from an awoken monitor fd
 * -> destroy_monitor() to free monitor resources
 * 
 * -> capture() that will actually capture frames from device
 * 
 * To add a new sensor, just insert a new define in _SENSORS; note that sensors are priority-ordered: lower int has higher priority.
 * Remeber that sensor's name should contain sensor's define stringified to actually be registered.
 **/

#include <commons.h>

#define _ctor_     __attribute__((constructor (101))) // Used for Sensors registering

/* Sensor->name must match its enumeration stringified value */
#define _SENSORS \
    X(ALS, 0) \
    X(YOCTOLIGHT, 1) \
    X(CAMERA, 2) \
    X(CUSTOM, 3) \
    X(SENSOR_NUM, 4)

enum sensors { 
#define X(name, val) name = val,
    _SENSORS
#undef X
};

typedef struct _sensor {
    const char *name;                       // actually sensor name -> note that exposed bus methods should have "name" in them
    bool (*validate_dev)(void *dev);
    void (*fetch_dev)(const char *subsystem, void **dev);
    void (*fetch_props_dev)(void *dev, const char **node, const char **action);
    void (*destroy_dev)(void *dev);
    int (*init_monitor)(void);
    void (*recv_monitor)(void **dev);
    void (*destroy_monitor)(void);
    int (*capture)(void *userdata, double *pct, const int num_captures, char *settings);
    char obj_path[100];
} sensor_t;

#define SENSOR(name) \
    static bool validate_dev(void *dev); \
    static void fetch_dev(const char *interface, void **dev); \
    static void fetch_props_dev(void *dev, const char **node, const char **action); \
    static void destroy_dev(void *dev); \
    static int init_monitor(void); \
    static void recv_monitor(void **dev); \
    static void destroy_monitor(void); \
    static int capture(void *dev, double *pct, const int num_captures, char *settings); \
    static void _ctor_ register_sensor(void) { \
        static sensor_t self = { name, validate_dev, fetch_dev, fetch_props_dev, destroy_dev, init_monitor, recv_monitor, destroy_monitor, capture }; \
        sensor_register_new(&self); \
    }

void sensor_register_new(sensor_t *sensor);
