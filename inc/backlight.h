#include "commons.h"

int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getactualbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_isinterface_enabled(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
