#ifdef DPMS_PRESENT

#include <module/module_easy.h>
#include <commons.h>
#include <polkit.h>
#include "dpms_plugins/xorg.h"
#include "dpms_plugins/tty.h"

static int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clightd/clightd/Dpms";
static const char bus_interface[] = "org.clightd.clightd.Dpms";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Get", "ss", "i", method_getdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Set", "ssi", "i", method_setdpms, SD_BUS_VTABLE_UNPRIVILEGED),
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
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
}

static void receive(const msg_t *msg, const void *userdata) {

}

static void destroy(void) {
    xorg_close();
    tty_close();
}

static int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int dpms_state = 0;
    if (display && strlen(display)) {
        dpms_state = xorg_get_dpms_state(display, xauthority);
    } else {
        dpms_state = tty_get_dpms_state(); 
    }
    if (dpms_state < 0) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to set dpms.");
        return dpms_state;
    }
    
    m_log("Current dpms state: %d.\n", dpms_state);
    return sd_bus_reply_method_return(m, "i", dpms_state);
}

static int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    int level;
    
    /* Require polkit auth */
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi", &display, &xauthority, &level);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* 0 -> DPMSModeOn, 3 -> DPMSModeOff */
    if (level < 0 || level > 3) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong DPMS level value.");
        return -EINVAL;
    }
    
    int err = 0;
    if (display && strlen(display)) {
        /* Xorg */
        err = xorg_set_dpms_state(display, xauthority, level);
    } else {
        /* tty */
        err = tty_set_dpms_state(level);
    }
    if (err) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to set dpms level.");
        return err;
    }
    
    m_log("New dpms state: %d.\n", level);
    return sd_bus_reply_method_return(m, "i", level);
}

#endif
