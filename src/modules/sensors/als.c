#include <sensor.h>

#define ALS_NAME            "Als"
#define ALS_ILL_MAX         500
#define ALS_ILL_MIN         22
#define ALS_INTERVAL        20 // ms
#define ALS_SUBSYSTEM       "iio"
#define ALS_SYSNAME_MATCH   "*als*" // match any "als" device

SENSOR(ALS_NAME, ALS_SUBSYSTEM, ALS_SYSNAME_MATCH);

/* Settings string unused */
static int capture(struct udev_device *dev, double *pct, const int num_captures, char *settings) {
    /* properties names to be checked. "in_illuminance_input" has higher priority. */
    static const char *ill_names[] = { "in_illuminance_input", "in_illuminance_raw" };
    
    int ret = -num_captures;
    /* Properly load scale value; defaults to 1.0 */
    double scale = 1.0;
    const char *val = udev_device_get_sysattr_value(dev, "in_illuminance_scale");
    if (val) {
        scale = atof(val);
    }
    
    for (int i = 0; i < num_captures; i++) {
        double illuminance = -1;
        for (int i = 0; i < SIZE(ill_names) && illuminance == -1; i++) {
            val = udev_device_get_sysattr_value(dev, ill_names[i]);
            if (val) {
                illuminance = atof(val) * scale;
                if (illuminance > ALS_ILL_MAX) {
                    illuminance = ALS_ILL_MAX;
                } else if (illuminance < ALS_ILL_MIN) {
                    illuminance = ALS_ILL_MIN;
                }
            }
        }
        
        if (illuminance >= 0) {
            pct[i] = illuminance / ALS_ILL_MAX;
            ret++;
        }
        
        usleep(ALS_INTERVAL * 1000);
    }
    return ret; // 0 if all requested captures are fullfilled
}
