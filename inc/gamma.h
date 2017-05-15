/**
 * Taken from sct.c
 * http://www.tedunangst.com/flak/post/sct-set-color-temperature
 */

#ifdef GAMMA_PRESENT

#include "commons.h"

void set_gamma(const char *display, const char *xauthority, int temp, int *err);
int get_gamma(const char *display, const char *xauthority, int *err);

#endif
