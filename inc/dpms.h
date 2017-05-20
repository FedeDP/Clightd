#ifdef DPMS_PRESENT

#include "commons.h"
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#define DPMS_DISABLED -1
#define NO_X -2

struct dpms_timeout {
    CARD16 standby;
    CARD16 suspend;
    CARD16 off;
};

int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_setdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

#endif
