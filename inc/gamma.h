/**
 * Taken from sct.c
 * http://www.tedunangst.com/flak/post/sct-set-color-temperature
 */

#ifdef GAMMA_PRESENT

#include "commons.h"

int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

#endif
