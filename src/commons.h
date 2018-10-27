#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <libudev.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <privilege.h>

#define _ctor0_     __attribute__((constructor (101))) // Used for Modules registering
#define _ctor1_     __attribute__((constructor (102))) // Used for Sensors registering

/* List of modules indexes */
enum modules { 
    BUS, 
    SIGNAL, 
    BACKLIGHT,
#ifdef GAMMA_PRESENT
    GAMMA, 
#endif
    SENSOR, 
#ifdef DPMS_PRESENT
    DPMS,
#endif
#ifdef IDLE_PRESENT
    IDLE, 
#endif
    MODULES_NUM };

enum quit_codes { LEAVE_W_ERR = -1, SIGNAL_RCV = 1 };

typedef struct module {
    const int idx;
    const char *name;
    int (*init)(void);                    // module init function
    void (*destroy)(void);                // module destroy function
    int (*poll_cb)(const int fd);        // module poll callback
} module_t;

sd_bus *bus;
struct udev *udev;
module_t modules[MODULES_NUM];
