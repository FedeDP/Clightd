#include "commons.h"

int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_setbrightnesspct(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightnesspct(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
#ifdef USE_DDC
int method_setbrightness_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_setbrightnesspct_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightness_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightnesspct_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
#endif
int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
