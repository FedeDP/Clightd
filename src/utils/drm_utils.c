#if defined GAMMA_PRESENT || defined DPMS_PRESENT

#include "drm_utils.h"
#include "commons.h"
#include "udev.h"

#define DRM_SUBSYSTEM "drm"

int drm_open_card(const char **card) {
    if (!*card || !strlen(*card)) {
        /* Fetch first matching device from udev */
        struct udev_device *dev = NULL;
        get_udev_device(NULL, DRM_SUBSYSTEM, NULL, NULL, &dev);
        if (!dev) {
            return -ENOENT;
        }
        
        /* Free old value */
        free((char *)*card);
        
        /* Store new value */
        *card = strdup(udev_device_get_devnode(dev));
        udev_device_unref(dev);
    }
    int fd = open(*card, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
    }
    return fd;
}

#endif
