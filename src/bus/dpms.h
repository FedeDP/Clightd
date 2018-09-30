#ifdef DPMS_PRESENT

#include <commons.h>

int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_setdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

#endif
