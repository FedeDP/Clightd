/**
 * Thanks to http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/ 
 * and to improvements made here: http://www.zombieprototypes.com/?p=210.
 **/

#ifdef GAMMA_PRESENT

#include <commons.h>
#include <polkit.h>
#include <module/map.h>
#include "gamma_plugins/xorg.h"
#include "gamma_plugins/drm.h"
#include "gamma_plugins/wl.h"

static void client_dtor(void *c);
static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static gamma_client *fetch_client(const char *display, const char *xauth);
static int start_client(gamma_client *sc, int temp, int smooth_step, int smooth_wait, int is_smooth);

static map_t *clients;
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
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    } else {
        clients = map_new(false, client_dtor);
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
        
        sd_bus_emit_signal(bus, object_path, bus_interface, "Changed", "si", sc->display, sc->current_temp);
        
        if (sc->handler.set(sc, sc->current_temp) == 0 && sc->current_temp == sc->target_temp) {
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
}

static void client_dtor(void *c) {
    gamma_client *cl = (gamma_client *)c;
    
    if (cl->handler.dtor) {
        cl->handler.dtor(cl);
    }
    free(cl->handler.priv);
    free(cl->display);
    free(cl);
}

static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int temp, error = 0;
    const char *display = NULL, *env = NULL;
    const int is_smooth;
    const unsigned int smooth_step, smooth_wait;
    
    ASSERT_AUTH();
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi(buu)", &display, &env, &temp, 
                                &is_smooth, &smooth_step, &smooth_wait);
    if (r < 0) {
        m_log("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    if (temp < 1000 || temp > 10000) {
        error = EINVAL;
    } else {
        gamma_client *sc = map_get(clients, display);
        if (!sc) {
            sc = fetch_client(display, env);
        }
        if (sc) {
            error = start_client(sc, temp, smooth_step, smooth_wait, is_smooth);
        } else {
            error = -EACCES;
        }
    }
    
    if (error) {
        if (error == EINVAL) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Temperature value should be between 1000 and 10000.");
        } else {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to open display handler plugin.");
        }
        return -error;
    }
    
    m_log("Temperature target value set (smooth %d): %d.\n", is_smooth, temp);
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
    
    gamma_client *cl = map_get(clients, display);
    if (cl) {
        temp = cl->current_temp;
    } else {
        cl = fetch_client(display, env);
        if (cl) {
            temp = cl->current_temp;
            client_dtor(cl);
        } else {
            error = -EACCES;
        }
    }
    
    if (error || temp == -1) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to get screen temperature.");
        return -error;
    }
    
    m_log("Current gamma value: %d.\n", temp);
    return sd_bus_reply_method_return(m, "i", temp);
}

static gamma_client *fetch_client(const char *display, const char *env) {
    gamma_client *cl = calloc(1, sizeof(gamma_client));
    if (cl) {
        cl->fd = -1;
        cl->display = strdup(display);
        int ret = xorg_get_handler(cl, env);
        if (ret == WRONG_PLUGIN) {
            ret = wl_get_handler(cl, env);
            if (ret == WRONG_PLUGIN) {
                ret = drm_get_handler(cl);
            }
        }
        if (ret != 0) {
            client_dtor(cl);
            cl = NULL;
        } else {
            cl->current_temp = cl->handler.get(cl);
        }
    }
    return cl;
}

static int start_client(gamma_client *cl, int temp, int smooth_step, int smooth_wait, int is_smooth) {
    cl->target_temp = temp;
    cl->smooth_step = smooth_step;
    cl->smooth_wait = smooth_wait;
    cl->is_smooth = is_smooth;
    
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
