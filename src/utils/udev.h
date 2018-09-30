#include <commons.h>

extern struct udev *udev;

int init_udev_monitor(const char *subsystem);
void receive_udev_device(struct udev_device **dev);
void get_udev_device(const char *interface, const char *subsystem, const char *sysname,
                     sd_bus_error **ret_error, struct udev_device **dev);
