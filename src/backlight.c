#include "../inc/backlight.h"
#include "../inc/polkit.h"
#include "../inc/udev.h"

#ifdef USE_DDC

#include <ddcutil_c_api.h>

#define FUNCTION_ERRMSG(function_name,status_code) \
fprintf(stderr, "(%s) %s() returned %d (%s): %s\n",      \
__func__, function_name, status_code,    \
ddca_rc_name(status_code),      \
ddca_rc_desc(status_code))

#define DDCUTIL_LOOP(func) \
const DDCA_Vcp_Feature_Code br_code = 0x10; \
DDCA_Display_Info_List *dlist = ddca_get_display_info_list(); \
for (int ndx = 0; ndx < dlist->ct; ndx++) { \
    DDCA_Display_Info *dinfo = &dlist->info[ndx]; \
    DDCA_Display_Ref dref = dinfo->dref; \
    DDCA_Display_Handle dh = NULL; \
    DDCA_Status rc; \
    rc = ddca_open_display(dref, &dh); \
    if (rc) { \
        FUNCTION_ERRMSG("ddca_open_display", rc); \
        continue; \
    } \
    DDCA_Single_Vcp_Value *valrec; \
    rc = ddca_get_vcp_value(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec); \
    if (rc) { \
        FUNCTION_ERRMSG("ddca_get_vcp_value", rc); \
        goto end_loop; \
    } \
    func; \
end_loop: \
    rc = ddca_close_display(dh); \
    if (rc) { \
        FUNCTION_ERRMSG("ddca_close_display", rc); \
    } \
} \
if (dlist) { \
    ddca_free_display_info_list(dlist); \
}

#endif

/**
 * Brightness setter method
 */
int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int value;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    int r = sd_bus_message_read(m, "si", &backlight_interface, &value);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    value = value > max ? max : value;
    value = value < 0 ? 0 : value;
    
    char val[10] = {0};
    sprintf(val, "%d", value);
    r = udev_device_set_sysattr_value(dev, "brightness", val);
    if (r < 0) {
        udev_device_unref(dev);
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_ACCESS_DENIED, "Not authorized.");
        return r;
    }
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "i", value);
}

/**
 * Brightness pct setter method
 */
int method_setbrightnesspct(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    double perc = 0.0;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    int r = sd_bus_message_read(m, "sd", &backlight_interface, &perc);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    perc = perc < 0.0 ? 0.0 : perc;
    perc = perc > 1.0 ? 1.0 : perc;
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    int value = perc * (double)max;
    char val[10] = {0};
    sprintf(val, "%d", value);
    r = udev_device_set_sysattr_value(dev, "brightness", val);
    if (r < 0) {
        udev_device_unref(dev);
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_ACCESS_DENIED, "Not authorized.");
        return r;
    }
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "d", perc);
}

#ifdef USE_DDC
int method_setbrightness_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int val;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    int r = sd_bus_message_read(m, "i", &val);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    val = val < 0 ? 0 : val;
    val = val > 100 ? 100 : val;
    
    DDCUTIL_LOOP({
        rc = ddca_set_continuous_vcp_value(dh, br_code, val);
        if (rc) {
            FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
        }
    });
    return sd_bus_reply_method_return(m, "i", val);
}

int method_setbrightnesspct_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    double perc = 0.0;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    int r = sd_bus_message_read(m, "d", &perc);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    perc = perc < 0.0 ? 0.0 : perc;
    perc = perc > 1.0 ? 1.0 : perc;
    
    DDCUTIL_LOOP({
        int new_value = valrec->val.c.max_val * perc;
        rc = ddca_set_continuous_vcp_value(dh, br_code, new_value);
        if (rc) {
            FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
        }
    });
    return sd_bus_reply_method_return(m, "d", perc);
}

int method_getbrightness_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "i");
    
    DDCUTIL_LOOP(sd_bus_message_append(reply, "i", valrec->val.c.cur_val));
    
    sd_bus_message_close_container(reply);
    int r = sd_bus_send(NULL, reply, NULL);
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
    }
    sd_bus_message_unref(reply);
    return r;
}

int method_getbrightnesspct_external(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "d");
    
    DDCUTIL_LOOP(sd_bus_message_append(reply, "d", (double)valrec->val.c.cur_val / valrec->val.c.max_val));
    
    sd_bus_message_close_container(reply);
    int r = sd_bus_send(NULL, reply, NULL);
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
    }
    sd_bus_message_unref(reply);
    return r;
}
#endif

/**
 * Current brightness getter method
 */
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    int r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));    
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "i", val);
}

/**
 * Current brightness pct getter method
 */
int method_getbrightnesspct(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    int r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
    int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    
    double pct = (double)val / max;    
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "d", pct);
}

/**
 * Max brightness value getter method
 */
int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    x = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));    
    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, "i", x);
}
