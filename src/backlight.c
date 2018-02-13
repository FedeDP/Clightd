#include "../inc/backlight.h"
#include "../inc/polkit.h"
#include "../inc/udev.h"

#ifdef USE_DDC

#include <ddcutil_c_api.h>

#define DDCUTIL_LOOP(func) \
    const DDCA_Vcp_Feature_Code br_code = 0x10; \
    DDCA_Display_Info_List *dlist = ddca_get_display_info_list(); \
    if (dlist) { \
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
                ddca_free_any_vcp_value(valrec); \
            } \
            ddca_close_display(dh); \
        } \
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
        ddca_free_any_vcp_value(valrec); \
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

typedef struct {
    int fd;
    double target_pct;
    double smooth_step;
    unsigned int smooth_wait;
    char interface[1024];
} smooth_change;

static int set_internal_backlight(const double pct, const char *path, const double smooth_step);
static int set_external_backlight(const double pct, const char *sn, const double smooth_step);
static int append_internal_backlight(sd_bus_message *reply, const char *path, int all);
static int append_external_backlight(sd_bus_message *reply, const char *sn, int all);

static smooth_change sc;

/**
 * Brightness pct setter method
 */
int method_setallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    const char *backlight_interface = NULL;
    const double target_pct = -1, smooth_step;
    const int is_smooth = 0;
    const unsigned int smooth_wait = 0;
        
    sd_bus_message_read(m, "ds", &target_pct, &backlight_interface);
    sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, "bdu");
    sd_bus_message_read(m, "bdu", &is_smooth, &smooth_step, &smooth_wait);
    sd_bus_message_exit_container(m);
    
    int ok = 0;
    if (is_smooth && smooth_step && smooth_wait) {
        sc.smooth_step = smooth_step;
        sc.smooth_wait = smooth_wait;
        sc.target_pct = target_pct;
        if (backlight_interface) {
            strncpy(sc.interface, backlight_interface, sizeof(sc.interface));
        } else {
            memset(sc.interface, 0, sizeof(sc.interface));
        }
        printf("Started backlight smooth transition, target: %lf\n", sc.target_pct);
        ok = brightness_smooth_cb();
    } else {
        printf("Setting backlight: %lf\n", target_pct);
        set_internal_backlight(target_pct, backlight_interface, 0);
        set_external_backlight(target_pct, NULL, 0);
    }
    
    // Returns true if no errors happened
    return sd_bus_reply_method_return(m, "b", ok == 0);
}

void set_brightness_smooth_fd(int fd) {
    sc.fd = fd;
}

int brightness_smooth_cb(void) {
    uint64_t t;
    // nonblocking mode!
    read(sc.fd, &t, sizeof(uint64_t));
    
    int ret = set_internal_backlight(sc.target_pct, sc.interface, sc.smooth_step);
    ret += set_external_backlight(sc.target_pct, NULL, sc.smooth_step);
    
    // set new timer if needed
    if (ret != -2) {
        struct itimerspec timerValue = {{0}};
        timerValue.it_value.tv_sec = 0;
        timerValue.it_value.tv_nsec = 1000 * 1000 * sc.smooth_wait; // ms
        return timerfd_settime(sc.fd, 0, &timerValue, NULL);
    }
    return 0;
}

static int set_internal_backlight(const double pct, const char *path, const double smooth_step) {
    int r = -1;
    struct udev_device *dev = NULL;
    get_udev_device(path, "backlight", NULL, &dev);
    if (dev) {
        double curr_pct;
        
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
        if (smooth_step > 0) {
            int curr = atoi(udev_device_get_sysattr_value(dev, "brightness"));
            curr_pct = curr / (double)max;
            if (pct < curr_pct) {
                curr_pct = (curr_pct - smooth_step < pct) ? pct : curr_pct - smooth_step;
            } else if (pct > curr_pct) {
                curr_pct = (curr_pct + smooth_step) > pct ? pct : curr_pct + smooth_step;
            } else {
                return -1;
            }
        } else {
            curr_pct = pct;
        }
        int value = curr_pct * max;
       
        char val[10] = {0};
        sprintf(val, "%d", value);
        r = udev_device_set_sysattr_value(dev, "brightness", val);
        udev_device_unref(dev);
    }
    return r;
}

static int set_external_backlight(const double pct, const char *sn, const double smooth_step) {
    int ret = -1;
    if (sn) {
        DDCUTIL_FUNC({
            int new_value = VALREC_MAX_VAL(valrec) * pct;
            ret = ddca_set_continuous_vcp_value(dh, br_code, new_value);
        });
    } else {
        ret = 0;
        DDCUTIL_LOOP({
            double curr_pct;
            const int max = VALREC_MAX_VAL(valrec);
            
            if (smooth_step > 0) {
                curr_pct = (double)VALREC_CUR_VAL(valrec) / max;
                if (pct < curr_pct) {
                    curr_pct = (curr_pct - smooth_step) < pct ? pct : curr_pct - smooth_step;
                } else if (pct > curr_pct) {
                    curr_pct = (curr_pct + smooth_step) > pct ? pct : curr_pct + smooth_step;
                } else {
                    curr_pct = -1;
                }
            } else {
                curr_pct = pct;
            }
            if (curr_pct != -1.0f) {
                int new_value = (double)max * curr_pct;
                ddca_set_continuous_vcp_value(dh, br_code, new_value);
                ret++; // number of screens
            }
        });
    }
    return ret;
}

/**
 * Backlight pct getter method: for each screen (both internal and external)
 * it founds, it will return a "(serialNumber, current backlight pct)" struct.
 * Note that for internal laptop screen, serialNumber = syspath (eg: intel_backlight)
 */
int method_getallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    const char *backlight_interface = NULL;
    
    int r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r >= 0) {
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
        
        append_internal_backlight(reply, backlight_interface, 1);
        append_external_backlight(reply, NULL, 1);
        
        sd_bus_message_close_container(reply);
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_message_unref(reply);
        sd_bus_message_exit_container(m);
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    }
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
