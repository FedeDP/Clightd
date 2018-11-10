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

typedef struct module {
    const int idx;
    const char *name;
    int (*init)(void);                    // module init function
    void (*destroy)(void);                // module destroy function
    int (*poll_cb)(const int fd);        // module poll callback
} module_t;

sd_bus *bus;
struct udev *udev;
