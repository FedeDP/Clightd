#include <commons.h>

int init_udev_monitor(const char *subsystem, struct udev_monitor **mon);
void get_udev_device(const char *interface, const char *subsystem, const char *sysattr_match,
                     sd_bus_error **ret_error, struct udev_device **dev);
