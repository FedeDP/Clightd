#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <libudev.h>
#include <unistd.h>
#include <sys/timerfd.h>

sd_bus *bus;
