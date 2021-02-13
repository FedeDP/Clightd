#include <commons.h>
#include <module/map.h>
#include <polkit.h>
#include <udev.h>

#ifdef DDC_PRESENT

#include <ddcutil_c_api.h>

/* Default value */
static DDCA_Vcp_Feature_Code br_code = 0x10;

void bl_store_vpcode(int code) {
    br_code = code;
}

static void bl_load_vpcode(void) {
    if (getenv("CLIGHTD_BL_VCP")) {
        br_code = strtol(getenv("CLIGHTD_BL_VCP"), NULL, 16);
    }
}

#define DDCUTIL_LOOP(func) \
    DDCA_Display_Info_List *dlist = NULL; \
    ddca_get_display_info_list2(false, &dlist); \
    if (dlist) { \
        bool leave = false; \
        for (int ndx = 0; ndx < dlist->ct && !leave; ndx++) { \
            DDCA_Display_Info *dinfo = &dlist->info[ndx]; \
            DDCA_Display_Ref dref = dinfo->dref; \
            DDCA_Display_Handle dh = NULL; \
            if (ddca_open_display2(dref, false, &dh)) { \
                continue; \
            } \
            DDCA_Any_Vcp_Value *valrec; \
            if (!ddca_get_any_vcp_value_using_explicit_type(dh, br_code, DDCA_NON_TABLE_VCP_VALUE, &valrec)) { \
                char id[32]; \
                get_info_id(id, sizeof(id), dinfo); \
                func; \
                ddca_free_any_vcp_value(valrec); \
            } \
            ddca_close_display(dh); \
        } \
        ddca_free_display_info_list(dlist); \
    }

#define DDCUTIL_FUNC(sn, func) \
    DDCA_Display_Identifier pdid = NULL; \
    DDCA_Display_Ref dref = NULL; \
    DDCA_Display_Handle dh = NULL; \
    DDCA_Any_Vcp_Value *valrec = NULL; \
    if (convert_sn_to_id(sn, &pdid)) { \
        goto end; \
    } \
    if (ddca_get_display_ref(pdid, &dref)) { \
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
    if (pdid) { \
        ddca_free_display_identifier(pdid); \
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
    
    static DDCA_Status convert_sn_to_id(const char *sn, DDCA_Display_Identifier *pdid) {
        int rc = ddca_create_mfg_model_sn_display_identifier(NULL, NULL, sn, pdid);
        if (rc == 0) {
            return rc;
        }
        int id;
        if (sscanf(sn, "/dev/i2c-%d", &id) == 1) {
            return ddca_create_busno_display_identifier(id, pdid);
        }
        if (sscanf(sn, "/dev/usb/hiddev%d", &id) == 1) {
            return ddca_create_usb_hiddev_display_identifier(id, pdid);
        }
        if (sscanf(sn, "%d", &id) == 1) {
            return ddca_create_dispno_display_identifier(id, pdid);
        }
    }

#else

#define DDCUTIL_LOOP(func) do {} while(0)
#define DDCUTIL_FUNC(sn, func) do {} while(0)

#endif

#define BL_SUBSYSTEM        "backlight"

typedef struct {
    struct udev_device *dev;
    char *sn;
    bool reached_target;
} device;

typedef struct {
    double curr_pct;
    double target_pct;
    double smooth_step;
    unsigned int smooth_wait;
    int smooth_fd;
    device d;
    double verse;
} smooth_client;

/* Helpers */
static void dtor_client(void *client);
static void reset_backlight_struct(smooth_client *sc, double target_pct, double initial_pct, bool is_smooth, 
                                   double smooth_step, unsigned int smooth_wait, int verse);
static int add_backlight_sn(double target_pct, bool is_smooth, double smooth_step, 
                            unsigned int smooth_wait, int verse, const char *sn, int internal);
static void sanitize_target_step(double *target_pct, double *smooth_step);
static int get_all_brightness(sd_bus_message *m, sd_bus_message **reply, sd_bus_error *ret_error);
static void next_backlight_level(smooth_client *sc);
static int set_internal_backlight(smooth_client *sc);
static int set_external_backlight(smooth_client *sc);
static int set_single_serial(double target_pct, bool is_smooth, double smooth_step, const unsigned int smooth_wait,
                             const char *serial, int verse);
static void append_backlight(sd_bus_message *reply, const char *name, const double pct);
static int append_internal_backlight(sd_bus_message *reply, const char *path);
static int append_external_backlight(sd_bus_message *reply, const char *sn, bool first_found);

/* Exposed */
static int method_setallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_raiseallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_lowerallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static map_t *running_clients;
static const char object_path[] = "/org/clightd/clightd/Backlight";
static const char bus_interface[] = "org.clightd.clightd.Backlight";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetAll", "d(bdu)s", "b", method_setallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetAll", "s", "a(sd)", method_getallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("RaiseAll", "d(bdu)s", "b", method_raiseallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("LowerAll", "d(bdu)s", "b", method_lowerallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Set", "d(bdu)s", "b", method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", "s", "(sd)", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Raise", "d(bdu)s", "b", method_raisebrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Lower", "d(bdu)s", "b", method_lowerbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "sd", 0),
    SD_BUS_VTABLE_END
};
static struct udev_monitor *mon;

MODULE("BACKLIGHT");

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
    running_clients = map_new(false, dtor_client);
    int r = sd_bus_add_object_vtable(bus,
                                 NULL,
                                 object_path,
                                 bus_interface,
                                 vtable,
                                 NULL);
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
    int fd = init_udev_monitor(BL_SUBSYSTEM, &mon);
    m_register_fd(fd, false, NULL);
}

static void receive(const msg_t *msg, const void *userdata) {
    uint64_t t;

    if (!msg->is_pubsub) {
        if (msg->fd_msg->userptr) {
            /* From smooth client */
            smooth_client *sc = (smooth_client *)msg->fd_msg->userptr;
            read(sc->smooth_fd, &t, sizeof(uint64_t));
            if (!sc->d.reached_target) {
                int ret;
                if (sc->d.dev) {
                    ret = set_internal_backlight(sc);
                } else {
                    ret = set_external_backlight(sc);
                }
                if (ret != 0) {
                    m_log("failed to set backlight for %s\n", sc->d.sn);
                }
            }
            
            /*
             * For external monitor: 
             * Emit signal now as we will never receive them from udev monitor.
             */
            if (!sc->d.dev) {
                sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "sd", sc->d.sn, sc->curr_pct);
            }
            
            if (!sc->d.reached_target) {
                struct itimerspec timerValue = {{0}};
                timerValue.it_value.tv_sec = sc->smooth_wait / 1000;
                timerValue.it_value.tv_nsec = 1000 * 1000 * (sc->smooth_wait % 1000); // ms
                timerfd_settime(sc->smooth_fd, 0, &timerValue, NULL);
            } else {
                m_log("%s reached target backlight: %s%.2lf.\n", sc->d.sn, sc->verse > 0 ? "+" : (sc->verse < 0 ? "-" : ""), sc->target_pct);
                map_remove(running_clients, sc->d.sn);
            }
        } else {
            /* From udev monitor, consume! */
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char *action = udev_device_get_action(dev);
                if (action && !strcmp(action, UDEV_ACTION_CHANGE)) {
                    int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
                    int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
                    const double pct = (double)val / max;
                    sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "sd", udev_device_get_sysname(dev), pct);
                }
                udev_device_unref(dev);
            }
        }
    }
}

static void destroy(void) {
    map_free(running_clients);
    udev_monitor_unref(mon);
}

static void dtor_client(void *client) {
    smooth_client *sc = (smooth_client *)client;
    /* Free all resources */
    m_deregister_fd(sc->smooth_fd); // this will automatically close it!
    free(sc->d.sn);
    if (sc->d.dev) {
        udev_device_unref(sc->d.dev);
    }
    free(sc);
}

static void reset_backlight_struct(smooth_client *sc, double target_pct, double initial_pct, bool is_smooth, 
                                   double smooth_step, unsigned int smooth_wait, int verse) {
    sc->smooth_step = is_smooth ? smooth_step : 0.0;
    sc->smooth_wait = is_smooth ? smooth_wait : 0;
    sc->target_pct = target_pct;
    if (initial_pct != -1) {
        sc->curr_pct = initial_pct;
    }
    sc->verse = verse;
    
    /* Only if not already there */
    if (sc->smooth_fd == 0) {
        sc->smooth_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        m_register_fd(sc->smooth_fd, true, sc);
    }
    
    struct itimerspec timerValue = {{0}};
    timerValue.it_value.tv_sec = 0;
    timerValue.it_value.tv_nsec = 1; // immediately
    timerfd_settime(sc->smooth_fd, 0, &timerValue, NULL);
}

static int add_backlight_sn(double target_pct, bool is_smooth, double smooth_step, 
                            unsigned int smooth_wait, int verse, const char *sn, int internal) {
    bool ok = !internal; // for external monitor -> always ok
    struct udev_device *dev = NULL;
    char *sn_id = (sn && strlen(sn)) ? strdup(sn) : NULL;
    double initial_pct = 0.0;
    
    /* Properly check internal interface exists before adding it */
    if (internal) {
        get_udev_device(sn_id, BL_SUBSYSTEM, NULL, NULL, &dev);
        if (dev) {
            ok = true;
            if (!sn_id) {
                sn_id = strdup(udev_device_get_sysname(dev));
            }
            int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
            int curr = atoi(udev_device_get_sysattr_value(dev, "brightness"));
            initial_pct = (double) curr / max;
        }

        if (internal == -1 && !ok) {
            DDCUTIL_LOOP({
                if (!sn_id) {
                    sn_id = strdup(id);
                }
                if (!strcmp(sn_id, id)) {
                    ok = true;
                    leave = true; // loop variable defined in DDCUTIL_LOOP
                    const uint16_t max = VALREC_MAX_VAL(valrec);
                    const uint16_t curr = VALREC_CUR_VAL(valrec);
                    initial_pct = (double)curr / max;
                }
            });
        }
    }
    /* When internal is false, initial_pct is set afterwards */

    if (ok) {
        smooth_client *sc = calloc(1, sizeof(smooth_client));
        reset_backlight_struct(sc, target_pct, initial_pct, is_smooth, smooth_step, smooth_wait, verse);
        sc->d.dev = dev;
        sc->d.sn = sn_id;
        sc->d.reached_target = false;

        map_put(running_clients, sc->d.sn, sc);
    } else {
        free(sn_id);
    }
    return ok ? 0 : -1;
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

static int get_all_brightness(sd_bus_message *m, sd_bus_message **reply, sd_bus_error *ret_error) {
    const char *backlight_interface = NULL;

    int r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r >= 0) {
        sd_bus_message_new_method_return(m, reply);
        sd_bus_message_open_container(*reply, SD_BUS_TYPE_ARRAY, "(sd)");

        int ret = append_internal_backlight(*reply, backlight_interface);
        ret += append_external_backlight(*reply, NULL, false);
        if (ret == -2) { // both returned -1
            r = -ENODEV;
            sd_bus_error_set_errno(ret_error, -r);
        } else {
            r = 0;
        }
        sd_bus_message_close_container(*reply);
    }
    return r;
}

static void next_backlight_level(smooth_client *sc) {
    double target_pct = sc->target_pct;
    if (sc->verse != 0) {
        target_pct = sc->curr_pct + (sc->verse * sc->target_pct);
        sanitize_target_step(&target_pct, NULL);
    } 
    if (sc->smooth_step > 0) {
        if (target_pct < sc->curr_pct) {
            sc->curr_pct = (sc->curr_pct - sc->smooth_step < target_pct) ? 
            target_pct : sc->curr_pct - sc->smooth_step;
        } else if (target_pct > sc->curr_pct) {
            sc->curr_pct = (sc->curr_pct + sc->smooth_step) > target_pct ? 
            target_pct : sc->curr_pct + sc->smooth_step;
        } else {
            sc->curr_pct = -1.0f; // useless
        }
    } else {
        sc->curr_pct = target_pct;
    }

    if (sc->curr_pct == target_pct || sc->curr_pct == -1.0f) {
        sc->d.reached_target = true;
    }
}

static int set_internal_backlight(smooth_client *sc) {
    int r = -1;

    if (sc->d.dev) {
        int max = atoi(udev_device_get_sysattr_value(sc->d.dev, "max_brightness"));
        next_backlight_level(sc);
        int value = sc->curr_pct * max;
        /* Check if next_backlight_level returned -1 */
        if (value >= 0) {
            char val[15] = {0};
            snprintf(val, sizeof(val) - 1, "%d", value);
            r = udev_device_set_sysattr_value(sc->d.dev, "brightness", val);
        }
    }
    return r;
}

static int set_external_backlight(smooth_client *sc) {
    int ret = -1;

    DDCUTIL_FUNC(sc->d.sn, {
        const uint16_t max = VALREC_MAX_VAL(valrec);
        next_backlight_level(sc);
        int16_t new_value = sc->curr_pct * max;
        int8_t new_sh = new_value >> 8;
        int8_t new_sl = new_value & 0xff;
        if (new_value >= 0 && ddca_set_non_table_vcp_value(dh, br_code, new_sh, new_sl) == 0) {
            ret = 0;
        }
    });
    return ret;
}

static int set_single_serial(double target_pct, bool is_smooth, double smooth_step, 
                             const unsigned int smooth_wait, const char *serial, int verse) {
    int r = -1;
    sanitize_target_step(&target_pct, &smooth_step);

    smooth_client *sc = NULL;
    if (serial && strlen(serial)) {
        sc = map_get(running_clients, serial);
        r = 0;
    }
    if (!sc) {
        // we do not know if this is an internal backlight, check both (-1)
        r = add_backlight_sn(target_pct, is_smooth, smooth_step, smooth_wait, verse, serial, -1);
    } else {
        reset_backlight_struct(sc, target_pct, -1, is_smooth, smooth_step, smooth_wait, verse);
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
    get_udev_device(path, BL_SUBSYSTEM, NULL, NULL, &dev);

    if (dev) {
        int val = atoi(udev_device_get_sysattr_value(dev, "brightness"));
        int max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
        const double pct = (double)val / max;
        
        append_backlight(reply, udev_device_get_sysname(dev), pct);
        udev_device_unref(dev);
        ret = 0;
    }
    return ret;
}

static int append_external_backlight(sd_bus_message *reply, const char *sn, bool first_found) {
    int ret = -1;
    if (sn && strlen(sn)) {
        DDCUTIL_FUNC(sn, {
            ret = 0;
            append_backlight(reply, sn, (double)VALREC_CUR_VAL(valrec) / VALREC_MAX_VAL(valrec));
        });
    } else {
        DDCUTIL_LOOP({
            ret = 0;
            append_backlight(reply, id, (double)VALREC_CUR_VAL(valrec) / VALREC_MAX_VAL(valrec));
            if (first_found) {
                leave = true; // loop variable defined in DDCUTIL_LOOP
            }
        });
    }
    return ret;
}

static int method_setallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    ASSERT_AUTH();

    const char *backlight_interface = NULL;
    double target_pct, smooth_step;
    const int is_smooth;
    const unsigned int smooth_wait;

    int r = sd_bus_message_read(m, "d(bdu)s", &target_pct, &is_smooth, &smooth_step,
                                &smooth_wait, &backlight_interface);
    if (r >= 0) {
        sanitize_target_step(&target_pct, &smooth_step);

        int verse = 0;
        if (userdata) {
            verse = *((int *)userdata);
        }

        /* Clear map */
        map_clear(running_clients);
        add_backlight_sn(target_pct, is_smooth, smooth_step, smooth_wait, verse, backlight_interface, 1);
        DDCUTIL_LOOP({
            add_backlight_sn(target_pct, is_smooth, smooth_step, smooth_wait, verse, id, 0);
            smooth_client *sc = map_get(running_clients, id);
            if (sc) {
                const uint16_t max = VALREC_MAX_VAL(valrec);
                const uint16_t curr = VALREC_CUR_VAL(valrec);
                sc->curr_pct = (double)curr / max;
            }
        });
        m_log("Target pct: %s%.2lf\n", verse > 0 ? "+" : (verse < 0 ? "-" : ""), target_pct);
        // Returns true if no errors happened; false if another client is already changing backlight
        r = sd_bus_reply_method_return(m, "b", true);
    }
    return r;
}

/**
 * Backlight pct getter method: for each screen (both internal and external)
 * it founds, it will return a "(uid, current backlight pct)" struct.
 * Note that for internal laptop screen, uid = syspath (eg: intel_backlight)
 */
static int method_getallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int r = get_all_brightness(m, &reply, ret_error);
    if (r == 0) {
        r = sd_bus_send(NULL, reply, NULL);
    }
    sd_bus_message_unref(reply);
    return r;
}

static int method_raiseallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int verse = 1;
    return method_setallbrightness(m, &verse, ret_error);
}

static int method_lowerallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int verse = -1;
    return method_setallbrightness(m, &verse, ret_error);
}

static int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    ASSERT_AUTH();
    
    const char *serial = NULL;
    double target_pct, smooth_step;
    const int is_smooth;
    const unsigned int smooth_wait;
    
    int r = sd_bus_message_read(m, "d(bdu)s", &target_pct, &is_smooth, &smooth_step, &smooth_wait, &serial);
    if (r >= 0) {
        int verse = 0;
        if (userdata) {
            verse = *((int *)userdata);
        }
        r = set_single_serial(target_pct, is_smooth, smooth_step, smooth_wait, serial, verse);
        if (r == -1) {
            r = -EINVAL;
            sd_bus_error_set_errno(ret_error, -r);
        } else {
            // Returns true if no errors happened;
            r = sd_bus_reply_method_return(m, "b", true);
        }
    }
    return r;
}

static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
   const char *sn = NULL;
   int r = sd_bus_message_read(m, "s", &sn);
    if (r >= 0) {
        sd_bus_message *reply = NULL;
        sd_bus_message_new_method_return(m, &reply);
        r = 0;
        if (append_internal_backlight(reply, sn) == -1) {
            if (append_external_backlight(reply, sn, true) == -1) {
                sd_bus_error_set_errno(ret_error, ENODEV);
                r = -ENODEV;
            }
        }
        if (!r) {
            r = sd_bus_send(NULL, reply, NULL);
        }
        sd_bus_message_unref(reply);
    }
    return r;
}

static int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int verse = 1;
    return method_setbrightness(m, &verse, ret_error);
}

static int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int verse = -1;
    return method_setbrightness(m, &verse, ret_error);
}
