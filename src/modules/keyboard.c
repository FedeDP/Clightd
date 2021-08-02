#include <polkit.h>
#include <udev.h>
#include <math.h>
#include <module/map.h>

#define KBD_SUBSYSTEM           "leds"
#define KBD_SYSNAME_MATCH       "kbd_backlight"

typedef struct {
    int max;
    char obj_path[100];
    sd_bus_slot *slot;          // vtable's slot
    struct udev_device *dev;
} kbd_t;

static void dtor_kbd(void *data);
static int kbd_new(struct udev_device *dev, void *userdata);
static inline int set_value(kbd_t *k, const char *sysattr, int value);
static map_ret_code set_brightness(void *userdata, const char *key, void *data);
static map_ret_code set_timeout(void *userdata, const char *key, void *data);
static map_ret_code append_backlight(void *userdata, const char *key, void *data);
static map_ret_code append_timeout(void *userdata, const char *key, void *data);
static int method_setkeyboard(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getkeyboard(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_settimeout(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_gettimeout(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int fetch_timeout(kbd_t *k);

MODULE("KEYBOARD");

static const char object_path[] = "/org/clightd/clightd/KbdBacklight";
static const char main_interface[] = "org.clightd.clightd.KbdBacklight";
static const sd_bus_vtable main_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "d", "b", method_setkeyboard, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", NULL, "a(sd)", method_getkeyboard, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetTimeout", "i", "b", method_settimeout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetTimeout", NULL, "a(si)", method_gettimeout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "sd", 0),
    SD_BUS_VTABLE_END
};
static const char bus_interface[] = "org.clightd.clightd.KbdBacklight.Server";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "d", "b", method_setkeyboard, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", NULL, "d", method_getkeyboard, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetTimeout", "i", "b", method_settimeout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetTimeout", NULL, "i", method_gettimeout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("MaxBrightness", "i", NULL, offsetof(kbd_t, max), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_SIGNAL("Changed", "d", 0),
    SD_BUS_VTABLE_END
};

static map_t *kbds;
static struct udev_monitor *mon;

static void module_pre_start(void) {
    
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    kbds = map_new(true, dtor_kbd);
    int r = sd_bus_add_object_vtable(bus,
                                 NULL,
                                 object_path,
                                 main_interface,
                                 main_vtable,
                                 NULL);
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    } else {
        const udev_match match = { .sysname = "*"KBD_SYSNAME_MATCH };
        udev_devices_foreach(KBD_SUBSYSTEM, &match, kbd_new, NULL);
        int fd = init_udev_monitor(KBD_SUBSYSTEM, &mon);
        m_register_fd(fd, false, NULL);
    }
}

static void receive(const msg_t *msg, const void *userdata) {
    if (msg->is_pubsub) {
        return;
    }
    struct udev_device *dev = udev_monitor_receive_device(mon);
    if (!dev) {
        return;
    }
    const char *key = udev_device_get_sysname(dev);
    if (strstr(key, KBD_SYSNAME_MATCH)) {
        const char *action = udev_device_get_action(dev);
        if (action) {
            if (!strcmp(action, UDEV_ACTION_ADD)) {
                // Register new interface
                kbd_new(dev, NULL);
            } else if (!strcmp(action, UDEV_ACTION_RM)) {
                // Remove the interface
                map_remove(kbds, key);
            } else if (!strcmp(action, UDEV_ACTION_CHANGE)) {
                // Changed event!
                /* Note: it seems like "change" udev signal is never triggered for kbd backlight though */                
                kbd_t *k = map_get(kbds, key);
                if (k) {
                    int curr = atoi(udev_device_get_sysattr_value(dev, "brightness"));
                    const double pct = (double)curr / k->max;
                        
                    // Emit on global object
                    sd_bus_emit_signal(bus, object_path, main_interface, "Changed", "sd", udev_device_get_sysname(dev), pct);
                    // Emit on specific object too!
                    sd_bus_emit_signal(bus, k->obj_path, bus_interface, "Changed", "d", pct);
                }
            }
        }
    }
    udev_device_unref(dev);
}

static void destroy(void) {
    udev_monitor_unref(mon);
    map_free(kbds);
}

static void dtor_kbd(void *data) {
    kbd_t *k = (kbd_t *)data;
    sd_bus_slot_unref(k->slot);
    udev_device_unref(k->dev);
    free(k);
}

static int kbd_new(struct udev_device *dev, void *userdata) {  
    kbd_t *k = calloc(1, sizeof(kbd_t));
    if (!k) {
        m_log("failed to malloc.\n");
        return -1;
    }
    
    k->max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    k->dev = udev_device_ref(dev);
    
    const char *name = udev_device_get_sysname(dev);
    
    /*
     * Substitute wrong chars, eg: dell::kbd_backlight -> dell__kbd_backlight
     * See spec: https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-marshaling-object-path
     */
    snprintf(k->obj_path, sizeof(k->obj_path) - 1, "%s/%s", object_path, name);
    char *ptr = NULL;
    while ((ptr = strchr(k->obj_path, ':'))) {
        *ptr = '_';
    }
    
    int r = sd_bus_add_object_vtable(bus, &k->slot, k->obj_path, bus_interface, vtable, k);
    if (r < 0) {
        m_log("Failed to add object vtable on path '%s': %d\n", k->obj_path, r);
        dtor_kbd(k);
    } else {
        map_put(kbds, name, k);
    }
    return 0;
}

static inline int set_value(kbd_t *k, const char *sysattr, int value) {
    char val[15] = {0};
    snprintf(val, sizeof(val) - 1, "%d", value);
    return udev_device_set_sysattr_value(k->dev, sysattr, val);
}

static map_ret_code set_brightness(void *userdata, const char *key, void *data) {
    double target_pct = *((double *)userdata);
    kbd_t *k = (kbd_t *)data;
    
    if (set_value(k, "brightness", (int)round(target_pct * k->max)) >= 0) {
         // Emit on global object
        sd_bus_emit_signal(bus, object_path, main_interface, "Changed", "sd", udev_device_get_sysname(k->dev), target_pct);
        // Emit on specific object too!
        sd_bus_emit_signal(bus, k->obj_path, bus_interface, "Changed", "d", target_pct);
        return MAP_OK;
    }
    return MAP_ERR;
}

static map_ret_code set_timeout(void *userdata, const char *key, void *data) {
    int value = *((int *)userdata);
    kbd_t *k = (kbd_t *)data;
    if (set_value(k, "stop_timeout", value) >= 0) {
        return MAP_OK;
    }
    return MAP_ERR;
}

static int method_setkeyboard(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    ASSERT_AUTH();

    double target_pct;
    int r = sd_bus_message_read(m, "d", &target_pct);
    if (r >= 0) {
        if (target_pct >= 0.0 && target_pct <= 1.0) {
            kbd_t *k = (kbd_t *)userdata;
            if (k) {
                r = set_brightness(&target_pct, NULL, k);
            } else {
                r = map_iterate(kbds, set_brightness, &target_pct);
            }
            return sd_bus_reply_method_return(m, "b", r >= 0);
        }
        sd_bus_error_set_errno(ret_error, EINVAL);
        return -EINVAL;
    }
    return r;
}

static int method_getkeyboard(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    kbd_t *k = (kbd_t *)userdata;
    if (k) {
        int curr = atoi(udev_device_get_sysattr_value(k->dev, "brightness"));
        const double pct = (double)curr / k->max;
        return sd_bus_reply_method_return(m, "d", pct);
    }
    sd_bus_message *reply = NULL;
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
    int r = map_iterate(kbds, append_backlight, reply);
    sd_bus_message_close_container(reply);
    if (r == 0) {
        r = sd_bus_send(NULL, reply, NULL);
    }
    sd_bus_message_unref(reply);
    return r;
}

static map_ret_code append_backlight(void *userdata, const char *key, void *data) {
    sd_bus_message *reply = (sd_bus_message *)userdata;
    
    kbd_t *k = (kbd_t *)data;
    int curr = atoi(udev_device_get_sysattr_value(k->dev, "brightness"));
    const double pct = (double)curr / k->max;
    
    sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sd");
    sd_bus_message_append(reply, "sd", key, pct);
    sd_bus_message_close_container(reply);
    
    return MAP_OK;
}

static int method_settimeout(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    ASSERT_AUTH();
    
    int timeout;
    int r = sd_bus_message_read(m, "i", &timeout);
    if (r >= 0) {
        if (timeout > 0) {
            kbd_t *k = (kbd_t *)userdata;
            if (k) {
                r = set_timeout(&timeout, NULL, k);
            } else {
                r = map_iterate(kbds, set_timeout, &timeout);
            }
            return sd_bus_reply_method_return(m, "b", r >= 0);
        }
        sd_bus_error_set_errno(ret_error, EINVAL);
        return -EINVAL;
    }
    return r;
}

static int method_gettimeout(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    kbd_t *k = (kbd_t *)userdata;
    if (k) {
        int tm = fetch_timeout(k);
        if (tm >= 0) {
            return sd_bus_reply_method_return(m, "i", tm);
        }
        return sd_bus_error_set_errno(ret_error, -tm);
    }
    sd_bus_message *reply = NULL;
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(si)");
    int r = map_iterate(kbds, append_timeout, reply);
    sd_bus_message_close_container(reply);
    if (r == 0) {
        r = sd_bus_send(NULL, reply, NULL);
    }
    sd_bus_message_unref(reply);
    return r;
}

static int fetch_timeout(kbd_t *k) {
    const char *timeout = udev_device_get_sysattr_value(k->dev, "stop_timeout");
    if (timeout) {
        int tm;
        char suffix = 's';
        if (sscanf(timeout, "%d%c", &tm, &suffix) >= 1) {
            const char suffixes[] = { 's', 'm', 'h' };
            for (int i = 0; i < SIZE(suffixes); i++) {
                if (suffix == suffixes[i]) {
                    if (i > 0) {
                        tm *= i * 60;
                    }
                    break;
                }
            }
            return tm;
        }
        return -EINVAL;
    }
    return -ENOENT;
}

static map_ret_code append_timeout(void *userdata, const char *key, void *data) {
    sd_bus_message *reply = (sd_bus_message *)userdata;
    
    kbd_t *k = (kbd_t *)data;
    int tm = fetch_timeout(k);
    if (tm >= 0) {
        sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "si");
        sd_bus_message_append(reply, "si", key, tm);
        sd_bus_message_close_container(reply);
    }
    return MAP_OK;
}
