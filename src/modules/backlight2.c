#include <polkit.h>
#include <udev.h>
#include <math.h>
#include <module/map.h>

typedef struct {
    double target_pct;
    double step;
    unsigned int wait;
} smooth_params_t;

typedef struct {
    smooth_params_t params;
    int fd;
} smooth_t;

typedef struct {
    int is_internal;
    void *dev;
    int max;
    char obj_path[100];
    const char *sn;
    sd_bus_slot *slot;
    smooth_t *smooth; // when != NULL -> smoothing
} bl_t;

static int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void stop_smooth(bl_t *bl);
static void bl_dtor(void *data);
static void emit_signals(bl_t *bl, double pct);
static int store_internal_device(struct udev_device *dev, void *userdata);
static int set_backlight_value(bl_t *bl, double *pct, double smooth_step);

static map_t *bls;
static struct udev_monitor *mon;
static const char object_path[] = "/org/clightd/clightd/Backlight2";
static const char bus_interface[] = "org.clightd.clightd.Backlight2.Server";
static const char main_interface[] = "org.clightd.clightd.Backlight2";
static const sd_bus_vtable main_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "d(du)", "b", method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", NULL, "a(sd)", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Raise", "d(du)", "b", method_raisebrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Lower", "d(du)", "b", method_lowerbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "sd", 0),
    SD_BUS_VTABLE_END
};
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "d(du)", "b", method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", NULL, "d", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Raise", "d(du)", "b", method_raisebrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Lower", "d(du)", "b", method_lowerbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("Max", "i", NULL, offsetof(bl_t, max), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Internal", "b", NULL, offsetof(bl_t, is_internal), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_SIGNAL("Changed", "d", 0),
    SD_BUS_VTABLE_END
};
static int verse;

MODULE("BACKLIGHT2");

#ifdef DDC_PRESENT

    #include <ddcutil_c_api.h>

    /* Default value */
    static DDCA_Vcp_Feature_Code br_code = 0x10;

    static void bl_load_vpcode(void) {
        if (getenv("CLIGHTD_BL_VCP")) {
            br_code = strtol(getenv("CLIGHTD_BL_VCP"), NULL, 16);
            m_log("Set 0x%x vcp code.\n", br_code);
        }
    }

    static void get_info_id(char *id, const int size, const DDCA_Display_Info *dinfo) {
        if (!strlen(dinfo->sn) || !strcasecmp(dinfo->sn, "Unspecified")) {
            switch(dinfo->path.io_mode) {
                case DDCA_IO_I2C:
                    snprintf(id, size, "/dev/i2c-%d", dinfo->path.path.i2c_busno);
                    break;
                case DDCA_IO_USB:
                    snprintf(id, size, "/dev/usb/hiddev%d", dinfo->path.path.hiddev_devno);
                    break;
                default:
                    snprintf(id, size, "%d", dinfo->dispno);
                    break;
            }
        } else {
            strncpy(id, dinfo->sn, size);
        }
    }
    
    static void store_external_devices(void) {
        DDCA_Display_Info_List *dlist = NULL;
        ddca_get_display_info_list2(false, &dlist);
        if (dlist) {
            for (int ndx = 0; ndx < dlist->ct; ndx++) {
                DDCA_Display_Info *dinfo = &dlist->info[ndx];
                DDCA_Display_Ref dref = dinfo->dref;
                DDCA_Display_Handle dh = NULL;
                if (ddca_open_display2(dref, false, &dh)) {
                    continue;
                }
                DDCA_Any_Vcp_Value *valrec;
                if (!ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) {
                    bl_t *d = calloc(1, sizeof(bl_t));
                    if (d) {
                        d->is_internal = false;
                        d->dev = dref;
                        char id[32];
                        get_info_id(id, sizeof(id), dinfo);
                        d->sn = strdup(id);
                        d->max = VALREC_MAX_VAL(valrec);
                        snprintf(d->obj_path, sizeof(d->obj_path) - 1, "%s/%s", object_path, d->sn);
                        int r = sd_bus_add_object_vtable(bus, &d->slot, d->obj_path, bus_interface, vtable, d);
                        if (r < 0) {
                            m_log("Failed to add object vtable on path '%s': %d\n", d->obj_path, r);
                            bl_dtor(d);
                        } else {
                            map_put(bls, d->sn, d);
                        }
                    }
                    ddca_free_any_vcp_value(valrec);
                }
                ddca_close_display(dh);
            }
            ddca_free_display_info_list(dlist);
        }
    }

#else

    static void store_external_devices(void) { }

#endif

#define BL_SUBSYSTEM        "backlight"

static void module_pre_start(void) {

}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
#ifdef DDC_PRESENT
    bl_load_vpcode();
#endif
    bls = map_new(false, bl_dtor);
    int r = sd_bus_add_object_vtable(bus,
                                 NULL,
                                 object_path,
                                 main_interface,
                                 main_vtable,
                                 NULL);
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
    
    // Store and create internal devices object paths
    udev_devices_foreach(BL_SUBSYSTEM, NULL, store_internal_device, NULL);
    // Store and create external devices object paths
    store_external_devices();
    // Register udev monitor for new internal devices
    int fd = init_udev_monitor(BL_SUBSYSTEM, &mon);
    m_register_fd(fd, false, NULL);
}

/* 
 * Note that ddcutil does not yet support monitor plug/unplug, 
 * thus we are not able to runtime detect newly added/removed monitors.
 */
static void receive(const msg_t *msg, const void *userdata) {
    uint64_t t;

    if (!msg->is_pubsub) {
        if (msg->fd_msg->userptr) {
            /* From smooth client */
            bl_t *bl = (bl_t *)msg->fd_msg->userptr;
            read(bl->smooth->fd, &t, sizeof(uint64_t));
                        
            int ret = set_backlight_value(bl, &bl->smooth->params.target_pct, bl->smooth->params.step);
            if (ret != 0) {
                m_log("failed to set backlight for %s\n", bl->sn);
                /* 
                 * failed to set backlight; stop right now to avoid endless loop:
                 * some external monitors report the capability to mange backlight
                 * but they fail instead, leaving us to an infinite loop.
                 */
                stop_smooth(bl);
            }
        } else {
            /* From udev monitor, consume! */
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char *id = udev_device_get_sysname(dev);
                const char *action = udev_device_get_action(dev);
                if (action) {
                    bl_t *bl = map_get(bls, id);
                    if (!strcmp(action, UDEV_ACTION_CHANGE) && bl) {
                        int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
                        const double pct = (double)val / bl->max;
                        emit_signals(bl, pct);
                    } else if (!strcmp(action, UDEV_ACTION_ADD) && !bl) {
                        if (store_internal_device(dev, NULL) != 0) {
                            m_log("failed to store new device.\n");
                        }
                    } else if (!strcmp(action, UDEV_ACTION_RM) && bl) {
                        map_remove(bls, id);
                    }
                }
                udev_device_unref(dev);
            }
        }
    }
}

static void destroy(void) {
    map_free(bls);
    udev_monitor_unref(mon);
}

static void stop_smooth(bl_t *bl) {
    if (bl->smooth) {
        m_deregister_fd(bl->smooth->fd); // this will automatically close it!
        free(bl->smooth);
        bl->smooth = NULL;
    }
}

static void bl_dtor(void *data) {
    bl_t *bl = (bl_t *)data;
    sd_bus_slot_unref(bl->slot);
    if (bl->is_internal) {
        udev_device_unref(bl->dev);
    }
    stop_smooth(bl);
    free((void *)bl->sn);
    free(bl);
}

static void emit_signals(bl_t *bl, double pct) {
    sd_bus_emit_signal(bus, object_path, main_interface, "Changed", "sd", bl->sn, pct);
    sd_bus_emit_signal(bus, bl->obj_path, bus_interface, "Changed", "d", pct);
}

static double get_internal_backlight(bl_t *bl) {
    int val = atoi(udev_device_get_sysattr_value(bl->dev, "brightness"));
    return (double)val / bl->max;
}

static double get_external_backlight(bl_t *bl) {
    double value = 0.0;
#ifdef DDC_PRESENT
    DDCA_Display_Handle dh = NULL;
    if (ddca_open_display2(bl->dev, false, &dh) == 0) {
        DDCA_Any_Vcp_Value *valrec = NULL;
        if (!ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) {
            value = (double)VALREC_CUR_VAL(valrec) / bl->max;
            ddca_free_any_vcp_value(valrec);
        }
        ddca_close_display(dh);
    }
#endif
    return value;
}

static map_ret_code get_backlight(void *userdata, const char *key, void *data) {
    bl_t *bl = (bl_t *)data;
    double *val = (double *)userdata; 
    if (bl->is_internal) {
       *val = get_internal_backlight(bl);
    } else {
        *val = get_external_backlight(bl);
    }
    return MAP_OK;
}

static int set_internal_backlight(bl_t *bl, int value) {
    char val[15] = {0};
    snprintf(val, sizeof(val) - 1, "%d", value);
    return udev_device_set_sysattr_value(bl->dev, "brightness", val);
}

static int set_external_backlight(bl_t *bl, int value) {
    int ret = -1;
#ifdef DDC_PRESENT
    DDCA_Display_Handle dh = NULL;
    if (!ddca_open_display2(bl->dev, false, &dh)) {
        int8_t new_sh = (value >> 8) & 0xff;
        int8_t new_sl = value & 0xff;
        ret = ddca_set_non_table_vcp_value(dh, br_code, new_sh, new_sl);
        ddca_close_display(dh);
    }
#endif
    return ret;
}

static int store_internal_device(struct udev_device *dev, void *userdata) {
    const int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    const char *id = udev_device_get_sysname(dev);
    bl_t *d = calloc(1, sizeof(bl_t));
    if (d) {
        d->is_internal = true;
        d->dev = udev_device_ref(dev);
        d->max = max;     
        d->sn = strdup(id);
        snprintf(d->obj_path, sizeof(d->obj_path) - 1, "%s/%s", object_path, d->sn);
        int r = sd_bus_add_object_vtable(bus, &d->slot, d->obj_path, bus_interface, vtable, d);
        if (r < 0) {
            m_log("Failed to add object vtable on path '%s': %d\n", d->obj_path, r);
            bl_dtor(d);
            return r;
        } 
        return map_put(bls, d->sn, d);
    }
    return -ENOMEM;
}

static void sanitize_target_step(double *target_pct, double *smooth_step) {
    if (target_pct) {
        if (*target_pct > 1.0) {
            *target_pct = 1.0;
        } else if (*target_pct < 0.0) {
            *target_pct = 0.0;
        }
    }

    if (smooth_step) {
        if (*smooth_step >= 1.0 || *smooth_step < 0.0) {
            *smooth_step = 0.0; // disable smoothing
        }
    }
}

static double next_backlight_pct(bl_t *bl, double *target_pct, double smooth_step) {
    double curr_pct = 0.0;
    get_backlight(&curr_pct, NULL, bl);
    
    if (verse != 0) {
        *target_pct = curr_pct + (verse * *target_pct);
        sanitize_target_step(target_pct, NULL);
    }
    if (smooth_step > 0) {
        if (*target_pct < curr_pct) {
            curr_pct = (curr_pct - smooth_step < *target_pct) ? *target_pct : curr_pct - smooth_step;
        } else if (*target_pct > curr_pct) {
            curr_pct = (curr_pct + smooth_step) > *target_pct ? *target_pct : curr_pct + smooth_step;
        } else {
            curr_pct = -1.0f; // useless
        }
    } else {
        curr_pct = *target_pct;
    }
    return curr_pct;
}

/* Set a target_pct eventually computing smooth step */
static int set_backlight_value(bl_t *bl, double *target_pct, double smooth_step) {
    const double next_pct = next_backlight_pct(bl, target_pct, smooth_step);
    if (next_pct == *target_pct || next_pct == -1.0f) {
        // end of smooth transition!
        m_log("%s reached target backlight: %.2lf.\n", bl->sn, next_pct);
        stop_smooth(bl);
    }
    const int value = (int)round(bl->max * next_pct);
    if (bl->is_internal) {
        return set_internal_backlight(bl, value);
    }
    int ret = set_external_backlight(bl, value);
    if (ret == 0) {
        /*
         * For external monitor: 
         * Emit signals now as we will never receive them from udev monitor.
         */
        emit_signals(bl, next_pct);
    }
    return ret;
}

static inline bool is_smooth(smooth_params_t *params) {
    return params->step > 0 && params->wait > 0;
}

static map_ret_code set_backlight(void *userdata, const char *key, void *data) {
    smooth_params_t *params = (smooth_params_t *)userdata;
    bl_t *bl = (bl_t *)data;
    
    stop_smooth(bl);
    if (set_backlight_value(bl, &params->target_pct, params->step) == 0) {
        if (is_smooth(params)) {
            bl->smooth = calloc(1, sizeof(smooth_t));
            if (bl->smooth) {
                memcpy(&bl->smooth->params, params, sizeof(smooth_params_t));
                
                // set and start timer fd
                bl->smooth->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
                m_register_fd(bl->smooth->fd, true, bl);
                struct itimerspec timerValue = {{0}};
                timerValue.it_value.tv_sec = params->wait / 1000;
                timerValue.it_value.tv_nsec = 1000 * 1000 * (params->wait % 1000); // ms
                timerValue.it_interval.tv_sec = params->wait / 1000;
                timerValue.it_interval.tv_nsec = 1000 * 1000 * (params->wait % 1000); // ms
                timerfd_settime(bl->smooth->fd, 0, &timerValue, NULL);
            }
        }
        return MAP_OK;
    }
    return MAP_ERR;
}

static int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    ASSERT_AUTH();
    
    smooth_params_t params = {0};
    int r = sd_bus_message_read(m, "d(du)", &params.target_pct, &params.step, &params.wait);
    if (r >= 0) {
        m_log("Target pct: %s%.2lf\n", verse > 0 ? "+" : (verse < 0 ? "-" : ""), params.target_pct);
        bl_t *d = (bl_t *)userdata;
        if (d) {
            r = set_backlight(&params, NULL, d);
        } else {
            r = map_iterate(bls, set_backlight, &params);
        }
        verse = 0; // reset verse
        r = sd_bus_reply_method_return(m, "b", r == 0);
    }
    return r;
}

static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {    
    bl_t *d = (bl_t *)userdata;
    double pct = 0.0;
    if (d) {
        get_backlight(&pct, NULL, d);
        return sd_bus_reply_method_return(m, "d", pct);
    }
    
    sd_bus_message *reply = NULL;
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
    for (map_itr_t *itr = map_itr_new(bls); itr; itr = map_itr_next(itr)) {
        bl_t *d = (bl_t *)map_itr_get_data(itr);
        const char *sn = map_itr_get_key(itr);
        get_backlight(&pct, NULL, d);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sd");
        sd_bus_message_append(reply, "sd", sn, pct);
        sd_bus_message_close_container(reply);
    }
    sd_bus_message_close_container(reply);
    sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return 0;
}

static int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    verse = 1;
    return method_setbrightness(m, userdata, ret_error);
}

static int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    verse = -1;
    return method_setbrightness(m, userdata, ret_error);
}
