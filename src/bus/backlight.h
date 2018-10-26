#include <commons.h>

int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

void set_brightness_smooth_fd(int fd);
int brightness_smooth_cb(void);
