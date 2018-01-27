#include "../inc/backlight.h"
#include "../inc/polkit.h"
#include "../inc/udev.h"

#ifdef USE_DDC

#include <ddcutil_c_api.h>

#define DDCUTIL_LOOP(func) \
    const DDCA_Vcp_Feature_Code br_code = 0x10; \
    DDCA_Display_Info_List *dlist = ddca_get_display_info_list(); \
    for (int ndx = 0; ndx < dlist->ct; ndx++) { \
        DDCA_Display_Info *dinfo = &dlist->info[ndx]; \
        DDCA_Display_Ref dref = dinfo->dref; \
        DDCA_Display_Handle dh = NULL; \
        if (ddca_open_display(dref, &dh)) { \
            continue; \
        } \
        DDCA_Any_Vcp_Value *valrec; \
        if (!ddca_get_any_vcp_value(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) { \
            func; \
        } \
        ddca_close_display(dh); \
    } \
    if (dlist) { \
        ddca_free_display_info_list(dlist); \
    }

#define DDCUTIL_FUNC(func) \
    const DDCA_Vcp_Feature_Code br_code = 0x10; \
    DDCA_Display_Identifier pdid = NULL; \
    DDCA_Display_Ref dref = NULL; \
    DDCA_Display_Handle dh = NULL; \
    DDCA_Any_Vcp_Value *valrec = NULL; \
    if (ddca_create_mfg_model_sn_display_identifier(NULL, NULL, sn, &pdid)) { \
        goto end; \
    } \
    if (ddca_create_display_ref(pdid, &dref)) { \
        goto end; \
    } \
    if (ddca_open_display(dref, &dh)) { \
        goto end; \
    } \
    if (!ddca_get_any_vcp_value(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) { \
        func; \
    } \
end: \
    if (dh) { \
        ddca_close_display(dh); \
    } \
    if (dref) { \
        ddca_free_display_ref(dref); \
    } \
    if (pdid) { \
        ddca_free_display_identifier(pdid); \
    }

#else

#define DDCUTIL_LOOP(func) do {} while(0)
#define DDCUTIL_FUNC(func) do {} while(0)

#endif

static int set_internal_backlight(const double pct, const char *path);
static int set_external_backlight(const double pct, const char *sn);
static int append_internal_backlight(sd_bus_message *reply, const char *path, int all);
static int append_external_backlight(sd_bus_message *reply, const char *sn, int all);

/**
 * Brightness pct setter method
 */
int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    int ok = 0;
    
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "(sd)");
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
    } else {
        while (sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, "sd") > 0) {
            const char *sn = NULL;
            double pct;
            if (sd_bus_message_read(m, "sd", &sn, &pct) > 0) {
                pct = pct < 0.0 ? 0.0 : pct;
                pct = pct > 1.0 ? 1.0 : pct;
                if (!set_internal_backlight(pct, sn) || !set_external_backlight(pct, sn)) {
                    ok++;
                }
            }
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
    }
    
    // Returns number of actually changed backlights
    return sd_bus_reply_method_return(m, "i", ok);
}

static int set_internal_backlight(const double pct, const char *path) {
    int r = -1;
    struct udev_device *dev = NULL;
    get_udev_device(path, "backlight", NULL, &dev);
    if (dev) {
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
        int value = pct * (double)max;
        char val[10] = {0};
        sprintf(val, "%d", value);
        r = udev_device_set_sysattr_value(dev, "brightness", val);
        udev_device_unref(dev);
    }
    
    return r;
}

static int set_external_backlight(const double pct, const char *sn) {
    int ret = -1;
    DDCUTIL_FUNC({
        int new_value = VALREC_MAX_VAL(valrec) * pct;
        ret = ddca_set_continuous_vcp_value(dh, br_code, new_value);
    });
    return ret;
}

/**
 * Backlight pct getter method: for each screen (both internal and external)
 * it founds, it will return a "(serialNumber, current backlight pct)" struct.
 * Note that for internal laptop screen, serialNumber = syspath (eg: intel_backlight)
 */
int method_getallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
        
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
        
    append_internal_backlight(reply, NULL, 1);
    append_external_backlight(reply, NULL, 1);
        
    sd_bus_message_close_container(reply);
    int r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    sd_bus_message_exit_container(m);
    return r;
}

/**
 * Current brightness pct getter method: for each serialNumber passed in,
 * it will return its backlight value in pct
 */
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        sd_bus_message *reply = NULL;
        
        const char *sn;
        
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "d");
        
        while (sd_bus_message_read(m, "s", &sn) > 0) {
            if (append_internal_backlight(reply, sn, 0)){
                append_external_backlight(reply, sn, 0);
            }
        }
        
        sd_bus_message_close_container(reply);
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_message_unref(reply);
        sd_bus_message_exit_container(m);
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    }
    return r;
}

static int append_internal_backlight(sd_bus_message *reply, const char *path, int all) {
    int ret = -1;
    struct udev_device *dev = NULL;
    get_udev_device(path, "backlight", NULL, &dev);
    
    if (dev) {
        int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    
        double pct = (double)val / max;
        if (!all) {
            sd_bus_message_append(reply, "d", pct);
        } else {
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sd");
            sd_bus_message_append(reply, "sd", udev_device_get_sysname(dev), pct);
            sd_bus_message_close_container(reply);
        }
        udev_device_unref(dev);
        ret = 0;
    } 
    return ret;
}

static int append_external_backlight(sd_bus_message *reply, const char *sn, int all) {
    int ret = -1;
    
    if (!all) {
        DDCUTIL_FUNC({
            sd_bus_message_append(reply, "d", (double)VALREC_CUR_VAL(valrec) / VALREC_MAX_VAL(valrec));
            ret = 0;
        });
    } else {
        DDCUTIL_LOOP({
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sd");
            sd_bus_message_append(reply, "sd", dinfo->sn, (double)VALREC_CUR_VAL(valrec) / VALREC_MAX_VAL(valrec));
            sd_bus_message_close_container(reply);
            ret = 0;
        });
    }
    return ret;
}
