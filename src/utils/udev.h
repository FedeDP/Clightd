#include <commons.h>

#define UDEV_RM_ACTION "remove"

typedef struct {
    const char *sysattr_key;
    const char *sysattr_val;
} udev_match;

int init_udev_monitor(const char *subsystem, struct udev_monitor **mon);
void get_udev_device(const char *interface, const char *subsystem, const udev_match *match,
                     sd_bus_error **ret_error, struct udev_device **dev);
