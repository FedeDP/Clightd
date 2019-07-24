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

/* Retain retrocompatibility with libmodule 4.X */
#if MODULE_VERSION_MAJ >= 5
#define DTOR_RET void
#define KEY     const char *key,
#else
#define DTOR_RET int
#define KEY
#endif

typedef struct module {
    const int idx;
    const char *name;
    int (*init)(void);                    // module init function
    void (*destroy)(void);                // module destroy function
    int (*poll_cb)(const int fd);        // module poll callback
} module_t;

sd_bus *bus;
struct udev *udev;
