#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <libudev.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <module/modules_easy.h>
#include <module/module_easy.h>

#define SIZE(x) (sizeof(x) / sizeof(*x))
#define _ctor_     __attribute__((constructor (101))) // Used for Sensors registering
#define _dtor_     __attribute__((destructor (101)))  // Used for libusb dtor

/* Used by dpms and gamma */
#define UNSUPPORTED             -1 
#define WRONG_PLUGIN            -2
#define COMPOSITOR_NO_PROTOCOL  -3

extern sd_bus *bus;
extern struct udev *udev;
