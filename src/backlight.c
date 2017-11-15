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

#endif

static int change_external_monitors_brightness(const double perc);

/**
 * Brightness setter method
 */
int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r;
    const double perc = 0.0;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "sd", &backlight_interface, &perc);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* Return an error if perc is < 0 || > 1 */
    if (perc < 0.0 || perc > 1.0) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Percentage cannot be lower than 0 or greater than 1.");
        return -EINVAL;
    }
    
    int num_changed = change_external_monitors_brightness(perc);
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (dev) {
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
        int value = perc * max;
        char val[10] = {0};
        sprintf(val, "%d", value);
        r = udev_device_set_sysattr_value(dev, "brightness", val);
        if (r >= 0) {
            printf("New brightness value for %s: %d\n", udev_device_get_sysname(dev), value);
            num_changed++;
        } else {
            perror("udev_device_set_sysattr_value");
        }
        udev_device_unref(dev);
    } else {
        // dont leave if no backlight controller is present as this can be a desktop pc
        sd_bus_error_set_errno(ret_error, 0);
    }

    /* return number of changed screens */
    return sd_bus_reply_method_return(m, "i", num_changed);
}

#ifdef USE_DDC
static int change_external_monitors_brightness(const double perc) {
    const DDCA_Vcp_Feature_Code br_code = 0x10;
    int ret = 0;
    
    DDCA_Display_Info_List *dlist = ddca_get_display_info_list();
    for (int ndx = 0; ndx <  dlist->ct; ndx++) {
        DDCA_Display_Info *dinfo = &dlist->info[ndx];
        DDCA_Display_Ref dref = dinfo->dref;
        DDCA_Display_Handle dh = NULL;
        DDCA_Status rc;
        
        rc = ddca_open_display(dref, &dh);
        if (rc) {
            FUNCTION_ERRMSG("ddca_open_display", rc);
            continue;
        }
        
        DDCA_Single_Vcp_Value *valrec;
        rc = ddca_get_vcp_value(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec);
        if (rc) {
            FUNCTION_ERRMSG("ddca_get_vcp_value", rc);
            goto end_loop;
        }
        
        int new_value = valrec->val.c.max_val * perc;
        rc = ddca_set_continuous_vcp_value(dh, br_code, new_value);
        if (rc) {
            FUNCTION_ERRMSG("ddca_set_continuous_vcp_value", rc);
            goto end_loop;
        }
        
        ret++;

end_loop:
        rc = ddca_close_display(dh);
        if (rc) {
            FUNCTION_ERRMSG("ddca_close_display", rc);
        }
    }
    
    if (dlist) {
        ddca_free_display_info_list(dlist);
    }
    
    return ret;
}
#else
static int change_external_monitors_brightness(const double perc) {
    return 0;
}
#endif

/**
 * Current brightness getter method
 */
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    x = atoi(udev_device_get_sysattr_value(dev, "brightness"));
    printf("Current brightness value for %s: %d\n", udev_device_get_sysname(dev), x);
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

/**
 * Max brightness value getter method
 */
int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
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
    printf("Max brightness value for %s: %d\n", udev_device_get_sysname(dev), x);
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

/**
 * Actual brightness value getter method
 */
int method_getactualbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    x = atoi(udev_device_get_sysattr_value(dev, "actual_brightness"));
    printf("Actual brightness value for %s: %d\n", udev_device_get_sysname(dev), x);
    
    udev_device_unref(dev);
    
    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}
