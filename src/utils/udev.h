#include <commons.h>

#define UDEV_ACTION_ADD     "add"
#define UDEV_ACTION_RM      "remove"
#define UDEV_ACTION_CHANGE  "change"

typedef struct {
    const char *sysattr_key;
    const char *sysattr_val;
    const char *sysname;
} udev_match;

int init_udev_monitor(const char *subsystem, struct udev_monitor **mon);
void get_udev_device(const char *interface, const char *subsystem, const udev_match *match,
                     sd_bus_error **ret_error, struct udev_device **dev);
void udev_devices_foreach(const char *subsystem, const udev_match *match,  
                          int (*cb)(struct udev_device *dev, void *userdata), void *userdata);
