#pragma once

#include <libudev.h>
#include "../inc/commons.h"

void get_first_matching_device(struct udev_device **dev, const char *subsystem);
void get_udev_device(const char *backlight_interface, const char *subsystem,
                     sd_bus_error **ret_error, struct udev_device **dev);
