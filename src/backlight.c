#include "../inc/backlight.h"
#include "../inc/polkit.h"
#include "../inc/udev.h"

#ifdef USE_DDC

#include <ddcutil_c_api.h>

#define DDCUTIL_LOOP(func) \
    const DDCA_Vcp_Feature_Code br_code = 0x10; \
    DDCA_Display_Info_List *dlist = NULL; \
    ddca_get_display_info_list2(false, &dlist); \
    if (dlist) { \
        for (int ndx = 0; ndx < dlist->ct; ndx++) { \
            DDCA_Display_Info *dinfo = &dlist->info[ndx]; \
            DDCA_Display_Ref dref = dinfo->dref; \
            DDCA_Display_Handle dh = NULL; \
            if (ddca_open_display2(dref, false, &dh)) { \
                continue; \
            } \
            DDCA_Any_Vcp_Value *valrec; \
            if (!ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) { \
                func; \
                ddca_free_any_vcp_value(valrec); \
            } \
            ddca_close_display(dh); \
        } \
        ddca_free_display_info_list(dlist); \
    }

#define DDCUTIL_FUNC(sn, func) \
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
    if (ddca_open_display2(dref, false, &dh)) { \
        goto end; \
    } \
    if (!ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) { \
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
#define DDCUTIL_FUNC(sn, func) do {} while(0)

#endif

typedef struct {
    char *sn;
    int reached_target;
} device;

typedef struct {
    int fd;
    double target_pct;
    double smooth_step;
    unsigned int smooth_wait;
    device *d;
    int num_dev;
    int all;
} smooth_change;

static void reset_backlight_struct(double target_pct, int is_smooth, double smooth_step, unsigned int smooth_wait, int all);
static void add_backlight_sn(const char *sn);
static int read_brightness_params(sd_bus_message *m, const double *target_pct, const int *is_smooth, 
                                  const double *smooth_step, const unsigned int *smooth_wait);
static int set_internal_backlight(int idx);
static int set_external_backlight(int idx);
static void append_backlight(sd_bus_message *reply, const char *name, const double pct);
static int append_internal_backlight(sd_bus_message *reply, const char *path);
static int append_external_backlight(sd_bus_message *reply, const char *sn);

static smooth_change sc;

static void reset_backlight_struct(double target_pct, int is_smooth, double smooth_step, unsigned int smooth_wait, int all) {
    if (sc.d) {
        for (int i = 0; i < sc.num_dev; i++) {
            free(sc.d[i].sn);
        }
        free(sc.d);
        sc.d = NULL;
    }
    sc.smooth_step = is_smooth ? smooth_step : 0;
    sc.smooth_wait = is_smooth ? smooth_wait : 0;
    sc.target_pct = target_pct;
    sc.all = all;
    sc.num_dev = 0;
}

static void add_backlight_sn(const char *sn) {
    sc.d = realloc(sc.d, sizeof(device) * ++sc.num_dev);
    sc.d[sc.num_dev - 1].reached_target = 0;
    if (sn) {
        sc.d[sc.num_dev - 1].sn = strdup(sn);
    } else {
        sc.d[sc.num_dev - 1].sn = NULL;
    }
}

static int read_brightness_params(sd_bus_message *m, const double *target_pct, const int *is_smooth, 
                                  const double *smooth_step, const unsigned int *smooth_wait) {
    // read d(bdu) params
    int r;
    if ((r = sd_bus_message_read(m, "d", target_pct)) >= 0 && 
        (r = sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, "bdu")) >= 0 && 
        (r = sd_bus_message_read(m, "bdu", is_smooth, smooth_step, smooth_wait)) >= 0 && 
        (r = sd_bus_message_exit_container(m)) >= 0) {
        return 0;
    }

    fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    return -r;
}

int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    const char *sn = NULL;
    const double target_pct = -1, smooth_step = 0;
    const int is_smooth = 0;
    const unsigned int smooth_wait = 0;
    
    int r = read_brightness_params(m, &target_pct, &is_smooth, &smooth_step, &smooth_wait);
    if (!r) {
        reset_backlight_struct(target_pct, is_smooth, smooth_step, smooth_wait, 0);
    
        // read backlight_interface/monitor id params
        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
        if (r > 0) {
            while (sd_bus_message_read(m, "s", &sn) > 0) {
                add_backlight_sn(sn);
            }
            sd_bus_message_exit_container(m);

            int ok = brightness_smooth_cb();
            // Returns true if no errors happened
            return sd_bus_reply_method_return(m, "b", !ok);
        }
    }
    return r;
}

int method_setallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    const char *backlight_interface = NULL;
    const double target_pct = -1, smooth_step = 0.0;
    const int is_smooth = 0;
    const unsigned int smooth_wait = 0;
    
    int r = read_brightness_params(m, &target_pct, &is_smooth, &smooth_step, &smooth_wait);
    if (!r) {
        r = sd_bus_message_read(m, "s", &backlight_interface);
    
        if (r >= 0) {
            reset_backlight_struct(target_pct, is_smooth, smooth_step, smooth_wait, 1);
            add_backlight_sn(backlight_interface);
            DDCUTIL_LOOP({
                add_backlight_sn(dinfo->sn);
            });
            int ok = brightness_smooth_cb();
            // Returns true if no errors happened
            return sd_bus_reply_method_return(m, "b", ok == 0);
        }
    }
    return r;
}

void set_brightness_smooth_fd(int fd) {
    sc.fd = fd;
}

int brightness_smooth_cb(void) {
    uint64_t t;
    // nonblocking mode!
    read(sc.fd, &t, sizeof(uint64_t));
    
    int reached_count = 0;
    for (int i = 0; i < sc.num_dev; reached_count += sc.d[i].reached_target, i++) {
        if (!sc.d[i].reached_target) {
            int ret = set_internal_backlight(i);
            // error: it was not an internal backlight interface
            if (ret == -1) {
                // try to use it as external backlight sn
                set_external_backlight(i);
            }
        }
    }
    
    struct itimerspec timerValue = {{0}};
    if (reached_count != sc.num_dev) {
        timerValue.it_value.tv_sec = 0;
        timerValue.it_value.tv_nsec = 1000 * 1000 * sc.smooth_wait; // ms
    } else {
        /* Free all resources */
        reset_backlight_struct(0, 0, 0, 0, 0);
    }
    /* disarm the timer */
    return timerfd_settime(sc.fd, 0, &timerValue, NULL);
}

static double next_backlight_level(int curr, int max, int idx) {
    double curr_pct;
    if (sc.smooth_step > 0) {
        curr_pct = curr / (double)max;
        if (sc.target_pct < curr_pct) {
            curr_pct = (curr_pct - sc.smooth_step < sc.target_pct) ? 
            sc.target_pct : curr_pct - sc.smooth_step;
        } else if (sc.target_pct > curr_pct) {
            curr_pct = (curr_pct + sc.smooth_step) > sc.target_pct ? 
            sc.target_pct : curr_pct + sc.smooth_step;
        } else {
            curr_pct = -1.0f;
        }
    } else {
        curr_pct = sc.target_pct;
    }
    
    if (idx >= 0 && curr_pct == sc.target_pct) {
        sc.d[idx].reached_target = 1;
    }
    
    return curr_pct;
}

static int set_internal_backlight(int idx) {
    int r = -1;
    
    struct udev_device *dev = NULL;
    get_udev_device(sc.d[idx].sn, "backlight", NULL, &dev);
    if (dev) {
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
        int curr = atoi(udev_device_get_sysattr_value(dev, "brightness"));
        int value = next_backlight_level(curr, max, idx) * max;
        /* Check if next_backlight_level returned -1 */
        if (value >= 0) {
            char val[10] = {0};
            sprintf(val, "%d", value);
            r = udev_device_set_sysattr_value(dev, "brightness", val);
        }
        udev_device_unref(dev);
    }
    return r;
}

static int set_external_backlight(int idx) {
    int ret = -1;
    
    DDCUTIL_FUNC(sc.d[idx].sn, {
        const uint16_t max = VALREC_MAX_VAL(valrec);
        const uint16_t curr = VALREC_CUR_VAL(valrec);
        int16_t new_value = next_backlight_level(curr, max, idx) * max;
        int8_t new_sh = new_value >> 8;
        int8_t new_sl = new_value & 0xff;
        if (new_value >= 0 && ddca_set_non_table_vcp_value(dh, br_code, new_sh, new_sl) == 0) {
            ret = 0;
        }
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
    const char *backlight_interface = NULL;
    
    int r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r >= 0) {
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
        
        append_internal_backlight(reply, backlight_interface);
        append_external_backlight(reply, NULL);
        
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
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
        
        while (sd_bus_message_read(m, "s", &sn) > 0) {
            if (append_internal_backlight(reply, sn)){
                append_external_backlight(reply, sn);
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

static void append_backlight(sd_bus_message *reply, const char *name, const double pct) {
    sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sd");
    sd_bus_message_append(reply, "sd", name, pct);
    sd_bus_message_close_container(reply);
}

static int append_internal_backlight(sd_bus_message *reply, const char *path) {
    int ret = -1;
    struct udev_device *dev = NULL;
    get_udev_device(path, "backlight", NULL, &dev);
    
    if (dev) {
        int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    
        double pct = (double)val / max;
        append_backlight(reply, udev_device_get_sysname(dev), pct);
        udev_device_unref(dev);
        ret = 0;
    } 
    return ret;
}

static int append_external_backlight(sd_bus_message *reply, const char *sn) {    
    if (sn) {
        DDCUTIL_FUNC(sn, {
            append_backlight(reply, sn, (double)VALREC_CUR_VAL(valrec) / VALREC_MAX_VAL(valrec));
        });
    } else {
        DDCUTIL_LOOP({
            append_backlight(reply, dinfo->sn, (double)VALREC_CUR_VAL(valrec) / VALREC_MAX_VAL(valrec));
        });
    }
    return 0;
}
