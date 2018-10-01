#include <sensor.h>

#define ILL_MAX         4096
#define ALS_SUBSYSTEM   "iio"
#define ALS_SYSNAME     "acpi-als"

SENSOR("als", ALS_SUBSYSTEM, "acpi-als");

static int capture(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct udev_device *dev = (struct udev_device *)userdata;
    int32_t illuminance = atoi(udev_device_get_sysattr_value(dev, "in_illuminance_input"));
    double pct = illuminance / ILL_MAX;
    return sd_bus_reply_method_return(m, "d", pct);
}
