#include "../inc/commons.h"

struct udev *udev;

void get_udev_device(const char *backlight_interface, const char *subsystem,
                     sd_bus_error **ret_error, struct udev_device **dev);
