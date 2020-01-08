#include <udev.h>
#include <regex.h>

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
    const char *udev_name_match;  // required udev name match (used by als sensor that REQUIRES "*als*" name, as "iio" subsystem alone is not enough to identify it)
    regex_t *udev_reg; // regex to match udev name
    int mon_handler;        // if an udev monitor is associated to this sensor, it will be != -1
    int (*capture_method)(struct udev_device *userdata, double *pct, const int num_captures, char *settings);
    char obj_path[100];
} sensor_t;

#define SENSOR(type, subsystem, udev_name_match) \
    static int capture(struct udev_device *dev, double *pct, const int num_captures, char *settings); \
    static void _ctor_ register_sensor(void) { \
		regex_t *regex = NULL; \
		if (udev_name_match) { \
			regex = calloc(1, sizeof(regex_t)); \
			regcomp(regex, udev_name_match, 0); \
		} \
		const sensor_t self = { type, subsystem, udev_name_match, regex, -1, capture }; \
        sensor_register_new(&self); \
    }

void sensor_register_new(const sensor_t *sensor);
