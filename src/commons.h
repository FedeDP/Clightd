#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef USE_STACK_T // TODO: drop when ugprading to libmodule6.0.0
#include <systemd/sd-bus.h>
#endif
#include <libudev.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <limits.h>
#include <stddef.h>
#include <module/modules_easy.h>
#include <module/module_easy.h>

#define SIZE(x) (sizeof(x) / sizeof(*x))

#define _ctor_     __attribute__((constructor (101))) // Used for plugins registering (sensor, gamma, dpms, screen) and libusb/libpipewire init
#define _dtor_     __attribute__((destructor (101)))  // Used for libusb and libpipewire dtor

/* Used by dpms, gamma and screen*/
#define UNSUPPORTED             INT_MIN 
#define WRONG_PLUGIN            INT_MIN + 1
#define COMPOSITOR_NO_PROTOCOL  INT_MIN + 2

#define UDEV_ACTION_ADD     "add"
#define UDEV_ACTION_RM      "remove"
#define UDEV_ACTION_CHANGE  "change"

#ifndef USE_STACK_T
extern sd_bus *bus;
#endif
extern struct udev *udev;
