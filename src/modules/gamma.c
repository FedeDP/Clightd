/**
 * Thanks to http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/ 
 * and to improvements made here: http://www.zombieprototypes.com/?p=210.
 **/

#ifdef GAMMA_PRESENT

#include <commons.h>
#include <polkit.h>
#include <module/map.h>
#include <math.h>
#include "gamma.h"
#include "bus_utils.h"

static unsigned short get_red(int temp);
static unsigned short get_green(int temp);
static unsigned short get_blue(int temp);
static void client_dtor(void *c);
static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static gamma_client *fetch_client(gamma_plugin *plugin, const char *display, const char *xauth, int *err);
static int start_client(gamma_client *sc, int temp, bool is_smooth, unsigned int smooth_step, unsigned int smooth_wait);

static map_t *clients;
static map_t *gamma_brightness;
static gamma_plugin *plugins[GAMMA_NUM];
static const char object_path[] = "/org/clightd/clightd/Gamma";
static const char bus_interface[] = "org.clightd.clightd.Gamma";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Set", "ssi(buu)", "b", method_setgamma, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", "ss", "i", method_getgamma, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "si", 0),
    SD_BUS_VTABLE_END
};

MODULE("GAMMA");

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
    
     for (int i = 0; i < GAMMA_NUM && !r; i++) {
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
    } else {
        clients = map_new(false, client_dtor);
        gamma_brightness = map_new(false, free);
    }
}

static void receive(const msg_t *msg, const void *userdata) {
    if (msg && !msg->is_pubsub) {
        uint64_t t;
        // nonblocking mode!
        read(msg->fd_msg->fd, &t, sizeof(uint64_t));
        gamma_client *sc = (gamma_client *)msg->fd_msg->userptr;
            
        if (sc->is_smooth) {
            if (sc->target_temp < sc->current_temp) {
                sc->current_temp = sc->current_temp - sc->smooth_step < sc->target_temp ? 
                sc->target_temp :
                sc->current_temp - sc->smooth_step;
            } else {
                sc->current_temp = sc->current_temp + sc->smooth_step > sc->target_temp ? 
                sc->target_temp :
                sc->current_temp + sc->smooth_step;
            }
        } else {
            sc->current_temp = sc->target_temp;
        }
        
        /* Emit signal on both /Gamma objpath, and /Gamma/$Plugin */
        sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "si", sc->display, sc->current_temp);
        sd_bus_emit_signal(bus, sc->plugin->obj_path, bus_interface, "Changed", "si", sc->display, sc->current_temp);
        
        if (sc->plugin->set(sc->priv, sc->current_temp) == 0 && sc->current_temp == sc->target_temp) {
            m_log("Reached target temp: %d.\n", sc->target_temp);
            m_deregister_fd(sc->fd); // this will close fd
            map_remove(clients, sc->display); // this will free sc->display (used as key)
        } else {
            struct itimerspec timerValue = {{0}};
            timerValue.it_value.tv_sec = sc->smooth_wait / 1000; // in ms
            timerValue.it_value.tv_nsec = 1000 * 1000 * (sc->smooth_wait % 1000); // ms
            timerfd_settime(sc->fd, 0, &timerValue, NULL);
        }
    }
}

static void destroy(void) {
    map_free(clients);
    map_free(gamma_brightness);
}

/** Exposed API in gamma.h **/
void gamma_register_new(gamma_plugin *plugin) {
    const char *plugins_names[] = {
    #define X(name, val) #name,
        _GAMMA_PLUGINS
    #undef X
    };
    
    int i;
    for (i = 0; i < GAMMA_NUM; i++) {
        if (strcasestr(plugins_names[i], plugin->name)) {
            break;
        }
    }
    
    if (i < GAMMA_NUM) {
        plugins[i] = plugin;
        printf("Registered '%s' gamma plugin.\n", plugin->name);
    } else {
        printf("Gamma plugin '%s' not recognized. Not registering.\n", plugin->name);
    }
}

double clamp(double x, double min, double max) {
    if (x < min) {
        return min;
    }
    if (x > max) {
        return max;
    }
    return x;
}

static unsigned short get_red(int temp) {
    if (temp <= 6500) {
        return 255;
    }
    const double a = 351.97690566805693;
    const double b = 0.114206453784165;
    const double c = -40.25366309332127;
    const double new_temp = ((double)temp / 100) - 55;
    
    return clamp(a + b * new_temp + c * log(new_temp), 0, 255);
}

static unsigned short get_green(int temp) {
    double a, b, c;
    double new_temp;
    if (temp <= 6500) {
        a = -155.25485562709179;
        b = -0.44596950469579133;
        c = 104.49216199393888;
        new_temp = ((double)temp / 100) - 2;
    } else {
        a = 325.4494125711974;
        b = 0.07943456536662342;
        c = -28.0852963507957;
        new_temp = ((double)temp / 100) - 50;
    }
    return clamp(a + b * new_temp + c * log(new_temp), 0, 255);
}

static unsigned short get_blue(int temp) {
    if (temp <= 1900) {
        return 0;
    }
    
    if (temp < 6500) {
        const double new_temp = ((double)temp / 100) - 10;
        const double a = -254.76935184120902;
        const double b = 0.8274096064007395;
        const double c = 115.67994401066147;
        
        return clamp(a + b * new_temp + c * log(new_temp), 0, 255);
    }
    return 255;
}

/* Thanks to: https://github.com/neilbartlett/color-temperature/blob/master/index.js */
int get_temp(const unsigned short R, const unsigned short B) {
    int temperature;
    int min_temp = B == 255 ? 6500 : 1000; // lower bound
    int max_temp = R == 255 ? 6500 : 10000; // upper bound
    unsigned short testR, testB;
    
    int ctr = 0;
    
    /* Compute first temperature with same R and B value as parameters */
    do {
        temperature = (max_temp + min_temp) / 2;
        testR = get_red(temperature);
        testB = get_blue(temperature);
        if ((double) testB / testR > (double) B / R) {
            max_temp = temperature;
        } else {
            min_temp = temperature;
        }
        ctr++;
    } while ((testR != R || testB != B) && (ctr < 10));
    
    /* try to fit value in 50-steps temp -> ie: instead of 5238, try 5200 or 5250 */
    if (temperature % 50 != 0) {
        int tmp_temp = temperature - temperature % 50;
        if (get_red(tmp_temp) == R && get_blue(tmp_temp) == B) {
            temperature = tmp_temp;
        } else {
            tmp_temp = temperature + 50 - temperature % 50;
            if (get_red(tmp_temp) == R && get_blue(tmp_temp) == B) {
                temperature = tmp_temp;
            }
        }
    }
    
    return temperature;
}

void fill_gamma_table(uint16_t *r, uint16_t *g, uint16_t *b, double br, uint32_t ramp_size, int temp) {
    const double red = get_red(temp) / (double)UINT8_MAX;
    const double green = get_green(temp) / (double)UINT8_MAX;
    const double blue = get_blue(temp) / (double)UINT8_MAX;
    
    for (uint32_t i = 0; i < ramp_size; ++i) {
        const double val = UINT16_MAX * i / ramp_size;
        r[i] = val * red * br;
        g[i] = val * green * br;
        b[i] = val * blue * br;
    }
}

void store_gamma_brightness(const char *id, double brightness) {
    if (!gamma_brightness) {
        return;
    }
    double *b = malloc(sizeof(double));
    *b = brightness;
    map_put(gamma_brightness, id, b);
}

double fetch_gamma_brightness(const char *id) {
    double *b = map_get(gamma_brightness, id);
    if (!b) {
        return 1.0;
    }
    return *b;
}

void clean_gamma_brightness(const char *id) {
    map_remove(gamma_brightness, id);
}

/* Utility function to refresh gamma levels. */
int refresh_gamma(void) {
    int error;
    gamma_client *sc = map_get(clients, NULL);
    if (!sc) {
        sc = fetch_client(NULL, "", "", &error);
    }
    if (sc) {
        return start_client(sc, sc->current_temp, false, 0, 0);
    }
    return -ENOENT;
}

/** **/

static void client_dtor(void *c) {
    gamma_client *cl = (gamma_client *)c;
    
    if (cl->plugin) {
        cl->plugin->dtor(cl->priv);
    }
    free((char *)cl->display);
    free((char *)cl->env);
    free(cl);
}

static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int temp, error = 0;
    const char *display = NULL, *env = NULL;
    const int is_smooth;
    const unsigned int smooth_step, smooth_wait;
    
    ASSERT_AUTH();
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi(buu)", &display, &env, &temp, &is_smooth, &smooth_step, &smooth_wait);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    if (temp < 1000 || temp > 10000) {
        error = EINVAL;
    } else {
        bus_sender_fill_creds(m);
        
        gamma_client *sc = map_get(clients, display);
        if (!sc) {
            sc = fetch_client(userdata, display, env, &error);
        }
        if (sc) {
            error = start_client(sc, temp, is_smooth, smooth_step, smooth_wait);
        }
    }
    
    if (error) {
        switch (error) {
        case EINVAL:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Temperature value should be between 1000 and 10000.");
            break;
        case COMPOSITOR_NO_PROTOCOL:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Compositor does not support wayland protocol.");
            break;
        case WRONG_PLUGIN:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "No plugin available for your configuration.");
            break;
        default:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to open display handler plugin.");
            break;
        }
        return -EACCES;
    }
    
    m_log("Temperature target value set: %d.\n", temp);
    return sd_bus_reply_method_return(m, "b", !error);
}

static int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int error = 0, temp = -1;
    const char *display = NULL, *env = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &env);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    bus_sender_fill_creds(m); // used by PW plugin
    
    gamma_client *cl = map_get(clients, display);
    if (cl) {
        temp = cl->current_temp;
    } else {
        cl = fetch_client(userdata, display, env, &error);
        if (cl) {
            temp = cl->current_temp;
            client_dtor(cl);
        }
    }
    
    if (error || temp == -1) {
        switch (error) {
        case COMPOSITOR_NO_PROTOCOL:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Compositor does not support 'wlr-gamma-control-unstable-v1' protocol.");
            break;
        case WRONG_PLUGIN:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "No plugin available for your configuration.");
            break;
        default:
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to get screen temperature.");
            break;
        }
        return -EACCES;
    }
    
    m_log("Current gamma value: %d.\n", temp);
    return sd_bus_reply_method_return(m, "i", temp);
}

static gamma_client *fetch_client(gamma_plugin *plugin, const char *display, const char *env, int *err) {
    gamma_client *cl = calloc(1, sizeof(gamma_client));
    if (cl) {
        cl->fd = -1;
        cl->display = strdup(display);
        cl->env = strdup(env);
        if (!plugin) {
            *err = WRONG_PLUGIN;
            for (int i = 0; i < GAMMA_NUM && *err == WRONG_PLUGIN; i++) {
                plugin = plugins[i];
                *err = plugin->validate(&cl->display, cl->env, &cl->priv);
            }
        } else {
            *err = plugin->validate(&cl->display, cl->env, &cl->priv);
        }
        if (*err != 0) {
            client_dtor(cl);
            cl = NULL;
        } else {
            cl->plugin = plugin;
            cl->current_temp = cl->plugin->get(cl->priv);
        }
    }
    return cl;
}

static int start_client(gamma_client *cl, int temp, bool is_smooth, unsigned int smooth_step, unsigned int smooth_wait) {
    cl->target_temp = temp;
    cl->is_smooth = is_smooth && smooth_step && smooth_wait;
    cl->smooth_step = smooth_step;
    cl->smooth_wait = smooth_wait;
    
    // NOTE: it seems like on wayland smooth transitions are not working.
    // Forcefully disable them for now.
    if (cl->plugin == plugins[WL] && cl->is_smooth) {
        fprintf(stderr, "Smooth transitions are not supported on wayland.\n");
        cl->is_smooth = false;
    }
    
    if (cl->fd == -1) {
        cl->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        m_register_fd(cl->fd, true, cl);
    }
            
    // start transitioning right now
    struct itimerspec timerValue = {{0}};
    timerValue.it_value.tv_nsec = 1;
    timerfd_settime(cl->fd, 0, &timerValue, NULL);
    return map_put(clients, cl->display, cl);
}

#endif
