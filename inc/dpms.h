#ifdef DPMS_PRESENT

#include "commons.h"
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#define DPMS_DISABLED -1

struct dpms_timeout {
    CARD16 standby;
    CARD16 suspend;
    CARD16 off;
};

int get_dpms_state(const char *display, const char *xauthority);
int set_dpms_state(const char *display, const char *xauthority, int dpms_level);
int get_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t);
int set_dpms_timeouts(const char *display, const char *xauthority, struct dpms_timeout *t);

#endif
