#ifdef DPMS_PRESENT

#include "dpms.h"
#include "polkit.h"

static int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static dpms_plugin *plugins[DPMS_NUM];
static const char object_path[] = "/org/clightd/clightd/Dpms";
static const char bus_interface[] = "org.clightd.clightd.Dpms";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Get", "ss", "i", method_getdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Set", "ssi", "b", method_setdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "si", 0),
    SD_BUS_VTABLE_END
};

MODULE("DPMS");

static void module_pre_start(void) {
    
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    int r = sd_bus_add_object_vtable(bus,
                                     NULL,
                                     object_path,
                                     bus_interface,
                                     vtable,
                                     NULL);
    for (int i = 0; i < DPMS_NUM && !r; i++) {
        if (plugins[i]) {
            snprintf(plugins[i]->obj_path, sizeof(plugins[i]->obj_path) - 1, "%s/%s", object_path, plugins[i]->name);
            r += sd_bus_add_object_vtable(bus,
                                        NULL,
                                        plugins[i]->obj_path,
                                        bus_interface,
                                        vtable,
                                        plugins[i]);
        }
    }
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
}

static void receive(const msg_t *msg, const void *userdata) {

}

static void destroy(void) {
    
}

void dpms_register_new(dpms_plugin *plugin) {
    const char *plugins_names[] = {
    #define X(name, val) #name,
        _DPMS_PLUGINS
    #undef X
    };
    
    int i;
    for (i = 0; i < DPMS_NUM; i++) {
        if (strcasestr(plugins_names[i], plugin->name)) {
            break;
        }
    }
    
    if (i < DPMS_NUM) {
        plugins[i] = plugin;
        printf("Registered '%s' dpms plugin.\n", plugin->name);
    } else {
        printf("Dpms plugin '%s' not recognized. Not registering.\n", plugin->name);
    }
}

static int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *env = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &env);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    dpms_plugin *plugin = userdata;
    int dpms_state = WRONG_PLUGIN;
    if (!plugin) {
        for (int i = 0; i < DPMS_NUM && dpms_state == WRONG_PLUGIN; i++) {
            dpms_state = plugins[i]->get(display, env);
        }
    } else {
        dpms_state = plugin->get(display, env);
    }
    if (dpms_state < 0) {
        if (dpms_state == COMPOSITOR_NO_PROTOCOL) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Compositor does not support wayland protocol.");
        } else if (dpms_state == WRONG_PLUGIN) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "No plugin available for your configuration.");
        } else {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to get dpms level.");
        }
        return -EACCES;
    }
    
    m_log("Current dpms state: %d.\n", dpms_state);
    return sd_bus_reply_method_return(m, "i", dpms_state);
}

static int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *env = NULL;
    int level;
    
   ASSERT_AUTH();
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi", &display, &env, &level);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* 0 -> DPMSModeOn, 3 -> DPMSModeOff */
    if (level < 0 || level > 3) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong DPMS level value.");
        return -EINVAL;
    }
    
    dpms_plugin *plugin = userdata;
    int err = WRONG_PLUGIN;
    if (!plugin) {
        for (int i = 0; i < DPMS_NUM && err == WRONG_PLUGIN; i++) {
            plugin = plugins[i];
            err = plugin->set(display, env, level);
        }
    } else {
        err = plugin->set(display, env, level);
    }
    if (err) {
        if (err == COMPOSITOR_NO_PROTOCOL) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Compositor does not support wayland protocol.");
        } else if (err == WRONG_PLUGIN) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "No plugin available for your configuration.");
        } else {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to set dpms level.");
        }
        return -EACCES;
    }
    
    m_log("New dpms state: %d.\n", level);
    sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "si", display, level);
    sd_bus_emit_signal(bus, plugin->obj_path, bus_interface, "Changed", "si", display, level);
    return sd_bus_reply_method_return(m, "b", true);
}

#endif
