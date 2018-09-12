#include "commons.h"

extern struct udev *udev;

#ifndef DISABLE_FRAME_CAPTURES
int init_udev_monitor(const char *subsystem);
void receive_udev_device(struct udev_device **dev);
#endif
void get_udev_device(const char *interface, const char *subsystem,
                     sd_bus_error **ret_error, struct udev_device **dev);
