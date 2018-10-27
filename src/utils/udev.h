#include <commons.h>

int init_udev_monitor(const char *subsystem, int *handler);
void receive_udev_device(struct udev_device **dev, int handler);
void get_udev_device(const char *interface, const char *subsystem, const char *sysname,
                     sd_bus_error **ret_error, struct udev_device **dev);
void destroy_udev_monitors(void);
