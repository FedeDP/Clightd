#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <libudev.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <module/modules_easy.h>
#include <module/module_easy.h>

#define SIZE(x) (sizeof(x) / sizeof(*x))
#define _ctor_     __attribute__((constructor (101))) // Used for Sensors registering
#define _dtor_     __attribute__((destructor (101)))  // Used for libusb dtor

extern sd_bus *bus;
extern struct udev *udev;
