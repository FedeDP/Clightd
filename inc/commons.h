#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <libudev.h>

struct udev *udev;
sd_bus *bus;
