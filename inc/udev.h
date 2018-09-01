#include "commons.h"

extern struct udev *udev;

void get_udev_device(const char *interface, const char *subsystem,
                     sd_bus_error **ret_error, struct udev_device **dev);
