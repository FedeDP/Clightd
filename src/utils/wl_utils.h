#if defined GAMMA_PRESENT || defined DPMS_PRESENT

#include <wayland-client.h>

struct wl_display *fetch_wl_display(const char *display, const char *env);

#endif
