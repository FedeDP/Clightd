#include <sensor.h>

#define ILL_MAX         4096
#define ALS_SUBSYSTEM   "iio"
#define ALS_SYSNAME     "acpi-als"

SENSOR("als", ALS_SUBSYSTEM, "acpi-als");

static int capture(struct udev_device *dev, double *pct) {
    int32_t illuminance = atoi(udev_device_get_sysattr_value(dev, "in_illuminance_input"));
    *pct = (double)illuminance / ILL_MAX;
    return 0;
}
