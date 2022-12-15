#ifdef SCREEN_PRESENT

#include "screen.h"
#include "bus_utils.h"

#define MONITOR_ILL_MAX              255

static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static screen_plugin *plugins[SCREEN_NUM];
static const char object_path[] = "/org/clightd/clightd/Screen";
static const char bus_interface[] = "org.clightd.clightd.Screen";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetEmittedBrightness", "ss", "d", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

MODULE("SCREEN");

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
    for (int i = 0; i < SCREEN_NUM && !r; i++) {
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

void screen_register_new(screen_plugin *plugin) {
    const char *plugins_names[] = {
    #define X(name, val) #name,
        _SCREEN_PLUGINS
    #undef X
    };
    
    int i;
    for (i = 0; i < SCREEN_NUM; i++) {
        if (strcasestr(plugins_names[i], plugin->name)) {
            break;
        }
    }
    
    if (i < SCREEN_NUM) {
        plugins[i] = plugin;
        printf("Registered '%s' screen plugin.\n", plugin->name);
    } else {
        printf("Screen plugin '%s' not recognized. Not registering.\n", plugin->name);
    }
}

int rgb_frame_brightness(const uint8_t *data, const int width, const int height, const int stride) {
    /*
     * takes 1 pixel every div*div area 
     * (div values > 8 will almost not give performance improvements)
     */
    int r = 0, g = 0, b = 0;
    const int div = 8;
    const int wmax = (double)width / div;
    const int hmax = (double)height / div;
    for (int i = 0; i < wmax; i++) {
        for (int k = 0; k < hmax; k++) {
            /* obtain r,g,b components */
            const uint8_t *p = data + (i * div + k * div * stride);
            r += p[2] & 0xFF;
            g += p[1] & 0xFF;
            b += p[0] & 0xFF;
        }
    }
    const int area = wmax * hmax;
    r = (double)r / area;
    g = (double)g / area;
    b = (double)b / area;
    /* return 0.299 * r + 0.587 * g + 0.114 * b; */
    /* https://en.wikipedia.org/wiki/Rec._709#luma_coefficients */
    /* https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.709-6-201506-I!!PDF-E.pdf */
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

static int method_getbrightness(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    const char *display = NULL, *env = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &env);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    bus_sender_fill_creds(m);
    
    screen_plugin *plugin = userdata;
    int br = WRONG_PLUGIN;
    if (!plugin) {
        for (int i = 0; i < SCREEN_NUM && br == WRONG_PLUGIN; i++) {
            br = plugins[i]->get(display, env);
        }
    } else {
        br = plugin->get(display, env);
    }

    if (br < 0) {
        switch (br) {
        case -EINVAL:
            sd_bus_error_set_errno(ret_error, -br);
            break;
        case COMPOSITOR_NO_PROTOCOL:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Compositor does not support 'wlr-screencopy-unstable-v1' protocol.");
            break;
        case WRONG_PLUGIN:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "No plugin available for your configuration.");
            break;
        case -EIO:
            sd_bus_error_set_errno(ret_error, EIO);
        }
        return -EACCES;
    }
    return sd_bus_reply_method_return(m, "d", (double)br / MONITOR_ILL_MAX);
}

#endif
