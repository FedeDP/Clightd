#if defined GAMMA_PRESENT || defined DPMS_PRESENT

#include <xf86drm.h>
#include <xf86drmMode.h>

int drm_open_card(const char *card_num);

#endif
