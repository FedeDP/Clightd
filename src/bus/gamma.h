#ifdef GAMMA_PRESENT

#include <commons.h>

int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

void set_gamma_smooth_fd(int fd);
int gamma_smooth_cb(void);

#endif
