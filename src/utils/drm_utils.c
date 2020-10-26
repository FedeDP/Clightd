#include "drm_utils.h"
#include "commons.h"

int drm_open_card(const char *card_num) {
    /* Default to "/dev/dri/card0" if empty */
    char card[PATH_MAX + 1];
    snprintf(card, sizeof(card), "/dev/dri/card%s", strlen(card_num) ? card_num : "0");
    
    int fd = open(card, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
    }
    return fd;
}
