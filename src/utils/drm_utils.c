#if defined GAMMA_PRESENT || defined DPMS_PRESENT

#include "drm_utils.h"
#include "commons.h"

#define DEFAULT_DRM "/dev/dri/card0"

int drm_open_card(const char *card) {
    if (!card || !strlen(card)) {
        card = DEFAULT_DRM;
    }
    int fd = open(card, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
    }
    return fd;
}

#endif
