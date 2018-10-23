#include <sensor.h>

#define ALS_ILL_MAX     4096
#define ALS_SUBSYSTEM   "iio"
#define ALS_SYSNAME     "acpi-als"

SENSOR("als", ALS_SUBSYSTEM, "acpi-als");

static int capture(struct udev_device *dev, double *pct, const int num_captures) {
    for (int i = 0; i < num_captures; i++) {
        int32_t illuminance = atoi(udev_device_get_sysattr_value(dev, "in_illuminance_input"));
        pct[i] = (double)illuminance / ALS_ILL_MAX;
    }
    return 0;
}
