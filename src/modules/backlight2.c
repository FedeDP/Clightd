#include <polkit.h>
#include <bus_utils.h>
#include <math.h>
#include "backlight.h"

/* Device manager */
static void stop_smooth(bl_t *bl);
static void bl_dtor(void *data);

/* Getters */
map_ret_code get_backlight(void *userdata, const char *key, void *data);

/* Setters */
static int set_backlight_value(bl_t *bl, double *target_pct, double smooth_step);
static map_ret_code set_backlight(void *userdata, const char *key, void *data);

/* Helper methods */
static void sanitize_target_step(double *target_pct, double *smooth_step);
static double next_backlight_pct(bl_t *bl, double *target_pct, double smooth_step);
static inline bool is_smooth(smooth_params_t *params);

/* DBus API */
// TODO: make these static once BACKLIGHT is killed
int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

map_t *bls;
static int verse;
static bl_plugin *plugins[BL_NUM];

static const char object_path[] = "/org/clightd/clightd/Backlight2";
static const char bus_interface[] = "org.clightd.clightd.Backlight2.Server";
static const char main_interface[] = "org.clightd.clightd.Backlight2";

/* To send signal on old interface. TODO: drop this once BACKLIGHT is killed */
static const char old_object_path[] = "/org/clightd/clightd/Backlight";
static const char old_bus_interface[] = "org.clightd.clightd.Backlight";

static const sd_bus_vtable main_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "d(du)", NULL, method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", NULL, "a(sd)", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Raise", "d(du)", NULL, method_raisebrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Lower", "d(du)", NULL, method_lowerbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "sd", 0),
    SD_BUS_VTABLE_END
};
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "d(du)", NULL, method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", NULL, "d", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Raise", "d(du)", NULL, method_raisebrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Lower", "d(du)", NULL, method_lowerbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("Max", "i", NULL, offsetof(bl_t, max), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Internal", "b", NULL, offsetof(bl_t, is_internal), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("DDC", "b", NULL, offsetof(bl_t, is_ddc), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Emulated", "b", NULL, offsetof(bl_t, is_emulated), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_SIGNAL("Changed", "d", 0),
    SD_BUS_VTABLE_END
};

MODULE("BACKLIGHT2");

static void module_pre_start(void) {

}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    bls = map_new(false, bl_dtor);
    sd_bus_add_object_manager(bus, NULL, object_path);
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
    for (int i = 0; i < BL_NUM; i++) {
        if (plugins[i]) {
            plugins[i]->load_env();
            plugins[i]->load_devices();
            int fd = plugins[i]->get_monitor();
            if (fd != -1) {
                m_register_fd(fd, false, plugins[i]);
            }
        }
    }
}

/* 
 * Note that ddcutil does not yet support monitor plug/unplug, 
 * thus we are not able to runtime detect newly added/removed monitors.
 */
static void receive(const msg_t *msg, const void *userdata) {
    uint64_t t;

    if (!msg->is_pubsub) {
        const void *ptr = msg->fd_msg->userptr;
        if (ptr != plugins[SYSFS] &&
            ptr != plugins[DDC]) {

            /* From smooth client */
            bl_t *bl = (bl_t *)ptr;
            read(bl->smooth->fd, &t, sizeof(uint64_t));

            int ret = set_backlight_value(bl, &bl->smooth->params.target_pct, bl->smooth->params.step);
            if (ret != 0) {
                m_log("failed to set backlight for %s\n", bl->sn);
                /* 
                 * failed to set backlight; stop right now to avoid endless loop:
                 * some external monitors report the capability to manage backlight
                 * but they fail instead, leaving us to an infinite loop.
                 */
                stop_smooth(bl);
            } else if (bl->smooth->params.target_pct == 0) {
                /* set_backlight_value advised us to stop smoothing as it ended */
                stop_smooth(bl);
            }
        } else {
            bl_plugin *pl = (bl_plugin *)ptr;
            pl->receive();
        }
    }
}

static void destroy(void) {
    for (int i = 0; i < BL_NUM; i++) {
        if (plugins[i]) {
            plugins[i]->dtor();
        }
    }
    map_free(bls);
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
    bl->plugin->free_device(bl);
    stop_smooth(bl);
    free((void *)bl->sn);
    free(bl);
}

/** backlight.h API **/
void bl_register_new(bl_plugin *plugin) {
    const char *plugins_names[] = {
        #define X(name, val) #name,
        _BL_PLUGINS
        #undef X
    };
    
    int i;
    for (i = 0; i < BL_NUM; i++) {
        if (strcasestr(plugins_names[i], plugin->name)) {
            break;
        }
    }
    
    if (i < BL_NUM) {
        plugins[i] = plugin;
        printf("Registered '%s' bl plugin.\n", plugin->name);
    } else {
        printf("Bl plugin '%s' not recognized. Not registering.\n", plugin->name);
    }
}

int store_device(bl_t *bl, enum backlight_plugins plenum) {
    bl->plugin = plugins[plenum];
    make_valid_obj_path(bl->obj_path, sizeof(bl->obj_path), object_path, bl->sn);
    int ret = sd_bus_add_object_vtable(bus, &bl->slot, bl->obj_path, bus_interface, vtable, bl);
    if (ret < 0) {
        m_log("Failed to add object vtable on path '%s': %d\n", bl->obj_path, ret);
        bl_dtor(bl);
    } else {
        ret = map_put(bls, bl->sn, bl);
        if (ret != 0){
            m_log("Failed to store new device.\n");
        }
    }
    return ret;
}

void emit_signals(bl_t *bl, double pct) {
    sd_bus_emit_signal(bus, object_path, main_interface, "Changed", "sd", bl->sn, pct);
    // TODO: drop this once BACKLIGHT is killed!
    sd_bus_emit_signal(bus, old_object_path, old_bus_interface, "Changed", "sd", bl->sn, pct);
    sd_bus_emit_signal(bus, bl->obj_path, bus_interface, "Changed", "d", pct);
}
/** Backlight.h API **/

map_ret_code get_backlight(void *userdata, const char *key, void *data) {
    bl_t *bl = (bl_t *)data;
    double *val = (double *)userdata;
    *val = (double)bl->plugin->get(bl) / bl->max;
    return MAP_OK;
}

/* Set a target_pct eventually computing smooth step */
static int set_backlight_value(bl_t *bl, double *target_pct, double smooth_step) {
    const double next_pct = next_backlight_pct(bl, target_pct, smooth_step);
    const int value = (int)round(bl->max * next_pct);
    int ret = bl->plugin->set(bl, value);
    if (ret == 0) {
        /*
         * For external monitor:
         * Emit signals now as we will never receive them from udev monitor.
         * For internal monitor:
         * Emit signals now or they will be filtered by receive() callback check
         * that new_val is != from cached one.
         */
        emit_signals(bl, next_pct);
    }
    if (next_pct == *target_pct) {
        m_log("%s reached target backlight: %.2lf.\n", bl->sn, next_pct);
        *target_pct = 0; // eventually disable smooth (if called by set_backlight and not by timerfd)
    }
    return ret;
}

static map_ret_code set_backlight(void *userdata, const char *key, void *data) {
    smooth_params_t *params = (smooth_params_t *)userdata;
    bl_t *bl = (bl_t *)data;
    
    stop_smooth(bl);
    
    const bool needs_smooth = is_smooth(params);
    if (set_backlight_value(bl, &params->target_pct, needs_smooth ? params->step : 0) == 0) {
        if (needs_smooth) {
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
    
    /* Manage Raise/Lower */
    if (verse != 0) {
        *target_pct = curr_pct + (verse * *target_pct);
        sanitize_target_step(target_pct, &smooth_step);
    }
    
    if (smooth_step > 0) {
        if (*target_pct < curr_pct) {
            curr_pct = (curr_pct - smooth_step < *target_pct) ? *target_pct : curr_pct - smooth_step;
        } else if (*target_pct > curr_pct) {
            curr_pct = (curr_pct + smooth_step) > *target_pct ? *target_pct : curr_pct + smooth_step;
        }
    } else {
        curr_pct = *target_pct;
    }
    return curr_pct;
}

static inline bool is_smooth(smooth_params_t *params) {
    return params->step > 0 && params->wait > 0 && params->target_pct > 0;
}

int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    ASSERT_AUTH();
    
    bus_sender_fill_creds(m);
    
    bool old_interface = false;
    smooth_params_t params = {0};
    int r = sd_bus_message_read(m, "d(du)", &params.target_pct, &params.step, &params.wait);
    if (r < 0) {
        // Try to read this as a BACKLIGHT request. TODO: drop once BACKLIGHT is killed!
        sd_bus_message_rewind(m, true);
        r = sd_bus_message_read(m, "d(bdu)", &params.target_pct, NULL, &params.step, &params.wait);
        old_interface = r >= 0;
    }
    if (r >= 0) {
        m_log("Target pct: %s%.2lf\n", verse > 0 ? "+" : (verse < 0 ? "-" : ""), params.target_pct);
        bl_t *d = (bl_t *)userdata;
        if (d) {
            r = set_backlight(&params, NULL, d);
        } else {
            r = map_iterate(bls, set_backlight, &params);
        }
        verse = 0; // reset verse
        if (!old_interface) {
            r = sd_bus_reply_method_return(m, NULL);
        } else {
            // TODO: drop once BACKLIGHT is killed!
            r = sd_bus_reply_method_return(m, "b", true);
        }
    }
    return r;
}

int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {    
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

int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    verse = 1;
    return method_setbrightness(m, userdata, ret_error);
}

int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    verse = -1;
    return method_setbrightness(m, userdata, ret_error);
}
