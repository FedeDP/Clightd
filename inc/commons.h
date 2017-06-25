#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <libudev.h>
#include <unistd.h>

sd_bus *bus;
