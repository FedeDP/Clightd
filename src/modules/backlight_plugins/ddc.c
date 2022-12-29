#ifdef DDC_PRESENT

#include <ddcutil_c_api.h>
#include <ddcutil_macros.h>
#include <udev.h>
#include <glob.h>
#include "gamma.h"
#include "backlight.h"

#define STR(x) _STR(x)
#define _STR(x) #x
#define ID_MAX_LEN 32
#define BL_VCP_ENV          "CLIGHTD_BL_VCP"
#define BL_EMULATED_ENV     "CLIGHTD_BL_EMULATED"
#define DRM_SUBSYSTEM       "drm"

static void add_new_external_display(const char *id, DDCA_Display_Info *dinfo, DDCA_Any_Vcp_Value *valrec);
static void get_ddc_id(char *id, const DDCA_Display_Info *dinfo);
static void get_emulated_id(char *id, int i2c_node);
static void update_external_devices(void);

BACKLIGHT("DDC");

static struct udev_monitor *drm_mon;
static DDCA_Vcp_Feature_Code br_code = 0x10;
static bool emulated_backlight_enabled = true;
static uint64_t curr_cookie;

static void load_env(void) {
    if (getenv(BL_VCP_ENV)) {
        br_code = strtol(getenv(BL_VCP_ENV), NULL, 16);
        printf("Overridden default vcp code: 0x%x\n", br_code);
    }
    if (getenv(BL_EMULATED_ENV)) {
        emulated_backlight_enabled = strtol(getenv(BL_EMULATED_ENV), NULL, 10);
        printf("Overridden default emulated backlight mode: %d.\n", emulated_backlight_enabled);
    }
    return;
}

static void load_devices(void) {
    DDCA_Display_Info_List *dlist = NULL;
    ddca_get_display_info_list2(true, &dlist);
    if (!dlist) {
        return;
    }
    for (int ndx = 0; ndx < dlist->ct; ndx++) {
        DDCA_Display_Info *dinfo = &dlist->info[ndx];
        DDCA_Display_Ref dref = dinfo->dref;
        DDCA_Display_Handle dh = NULL;
        if (ddca_open_display2(dref, false, &dh)) {
            continue;
        }
        DDCA_Any_Vcp_Value *valrec;
        char id[ID_MAX_LEN];
        int ret = ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec);
        if (ret == 0) {
            get_ddc_id(id, dinfo);
            add_new_external_display(id, dinfo, valrec);
            ddca_free_any_vcp_value(valrec);
        } else if (emulated_backlight_enabled && strlen(dinfo->model_name)) {
            // Laptop internal displays have got empty model name; skip them.
            get_emulated_id(id, dinfo->path.path.i2c_busno);
            add_new_external_display(id, dinfo, NULL);
        }
        ddca_close_display(dh);
    }
    ddca_free_display_info_list(dlist);
}

static int get_monitor(void) {
    return init_udev_monitor(DRM_SUBSYSTEM, &drm_mon);
}

static void receive(void) {
    struct udev_device *dev = udev_monitor_receive_device(drm_mon);
    if (dev) {
        // The event was from external monitor!
        update_external_devices();
        udev_device_unref(dev);
    }
}

static int set(bl_t *dev, int value) {
    if (dev->is_emulated) {
        store_gamma_brightness(dev->sn, (double)value / dev->max);
        return refresh_gamma();
    }
    int ret = -1;
    DDCA_Display_Handle dh = NULL;
    if (!ddca_open_display2(dev->dev, false, &dh)) {
        DDCA_Vcp_Feature_Code specific_br_code;
        char specific_br_env[64];
        snprintf(specific_br_env, sizeof(specific_br_env), BL_VCP_ENV"_%s", dev->sn);
        if (getenv(specific_br_env)) {
            specific_br_code = strtol(getenv(specific_br_env), NULL, 16);
        } else {
            specific_br_code = br_code;
        }
        int8_t new_sh = (value >> 8) & 0xff;
        int8_t new_sl = value & 0xff;
        ret = ddca_set_non_table_vcp_value(dh, specific_br_code, new_sh, new_sl);
        ddca_close_display(dh);
    }
    return ret;
}

static int get(bl_t *dev) {
    if (dev->is_emulated) {
        return fetch_gamma_brightness(dev->sn) * dev->max;
    }
    int value = 0;
    DDCA_Display_Handle dh = NULL;
    if (ddca_open_display2(dev->dev, false, &dh) == 0) {
        DDCA_Any_Vcp_Value *valrec = NULL;
        if (!ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) {
            value = VALREC_CUR_VAL(valrec);
            ddca_free_any_vcp_value(valrec);
        }
        ddca_close_display(dh);
    }
    return value;
}

static void free_device(bl_t *dev) {
    if (dev->is_emulated) {
        clean_gamma_brightness(dev->sn);
    }
}

static void dtor(void) {
    udev_monitor_unref(drm_mon);
}

static void add_new_external_display(const char *id, DDCA_Display_Info *dinfo, DDCA_Any_Vcp_Value *valrec) {
    bl_t *d = map_get(bls, id);
    if (!d) {
        d = calloc(1, sizeof(bl_t));
        if (d) {
            d->dev = dinfo->dref;
            d->sn = strdup(id);
            d->cookie = curr_cookie;
            if (valrec != NULL) {
                d->max = VALREC_MAX_VAL(valrec);
                d->is_ddc = true;
            } else {
                d->max = 100; // perc
                d->is_emulated = true;
            }
            store_device(d, DDC);
        }
    } else {
        // Update cookie and dref
        d->cookie = curr_cookie;
        d->dev = dinfo->dref;
    }
}

static void get_ddc_id(char *id, const DDCA_Display_Info *dinfo) {
    if ((dinfo->sn[0] == '\0') || !strcasecmp(dinfo->sn, "Unspecified")) {
        switch(dinfo->path.io_mode) {
            case DDCA_IO_I2C:
                snprintf(id, ID_MAX_LEN, "/dev/i2c-%d", dinfo->path.path.i2c_busno);
                break;
            case DDCA_IO_USB:
                snprintf(id, ID_MAX_LEN, "/dev/usb/hiddev%d", dinfo->path.path.hiddev_devno);
                break;
            default:
                snprintf(id, ID_MAX_LEN, "%d", dinfo->dispno);
                break;
        }
    } else {
        strncpy(id, dinfo->sn, ID_MAX_LEN);
    }
}

static void get_emulated_id(char *id, int i2c_node) {
    glob_t gl = {0};
    if (glob("/sys/class/drm/card*-*", GLOB_NOSORT | GLOB_ERR, NULL, &gl) == 0) {
        for (int i = 0; i < gl.gl_pathc; i++) {
            char path[PATH_MAX + 1];
            snprintf(path, sizeof(path), "%s/ddc/i2c-dev/i2c-%d", gl.gl_pathv[i], i2c_node);
            if (access(path, F_OK) == 0) {
                sscanf(gl.gl_pathv[i], "/sys/class/drm/card%*d-%"STR(ID_MAX_LEN)"s", id);
                break;
            }
        }
        globfree(&gl);
    }
}

static void update_external_devices(void) {
#if DDCUTIL_VMAJOR >= 1 && DDCUTIL_VMINOR >= 2
    /*
     * Algo: increment current cookie,
     * then rededect all displays.
     * Then, store any new external monitor with new cookie,
     * or update cookie and dref for still existent ones.
     * Finally, remove any non-existent monitor 
     * (look for external monitors whose cookie is != of current cookie)
     */
    curr_cookie++;
    ddca_redetect_displays();
    load_devices();
    for (map_itr_t *itr = map_itr_new(bls); itr; itr = map_itr_next(itr)) {
        bl_t *d = map_itr_get_data(itr);
        if (!d->is_internal && d->cookie != curr_cookie) {
            map_itr_remove(itr);
        }
    }
#endif
}

#endif
