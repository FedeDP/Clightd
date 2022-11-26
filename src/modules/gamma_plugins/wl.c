#define USE_STACK_T // this is needed for a bug in libmodule: stack_t type is already declared in signal.h

#include "wlr-gamma-control-unstable-v1-client-protocol.h"
#include "gamma.h"
#include "wl_utils.h"
#include <module/stack.h>

struct output {
    struct wl_output *wl_output;
    struct zwlr_gamma_control_v1 *gamma_control;
    uint32_t ramp_size;
    int table_fd;
    uint16_t *table;
    struct wl_list link;
};

typedef struct {
    struct wl_display *dpy;
    struct wl_list outputs;
    struct wl_registry *registry;
    struct zwlr_gamma_control_manager_v1 *gamma_control_manager;
    bool not_first_time;
} wlr_gamma_priv;

static int create_gamma_table(uint32_t ramp_size, uint16_t **table);
static void destroy_output(struct output *output);
static void gamma_control_handle_gamma_size(void *data, 
                                            struct zwlr_gamma_control_v1 *gamma_control, uint32_t ramp_size);
static void gamma_control_handle_failed(void *data,
                                        struct zwlr_gamma_control_v1 *gamma_control);
static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version);
static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry, uint32_t name);

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
    .gamma_size = gamma_control_handle_gamma_size,
    .failed = gamma_control_handle_failed,
};

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static stack_t *clients;
static bool leaving;

GAMMA("Wl");

MODULE("GAMMASWAY");

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    clients = stack_new(dtor);
}

static void receive(const msg_t *msg, const void *userdata) {
    if (msg && !msg->is_pubsub) {
        wlr_gamma_priv *priv = (wlr_gamma_priv *)msg->fd_msg->userptr;
        wl_display_dispatch(priv->dpy);
    }
}

static void destroy(void) {
    leaving = true; // notify dtor that all clients must be destroyed
    stack_free(clients);
}

static int validate(const char **id, const char *env,  void **priv_data) {
    struct wl_display *display = fetch_wl_display(*id, env);
    if (display == NULL) {
        return WRONG_PLUGIN;
    }
    
    /* Check if we already have a running client for this display */
    for (stack_itr_t *itr = stack_itr_new(clients); itr; itr = stack_itr_next(itr)) {
        wlr_gamma_priv *p = (wlr_gamma_priv *)stack_itr_get_data(itr);
        if (p->dpy == display) {
            *priv_data = p;
            // this is not the first time we use this client.
            // No need to register its fd
            p->not_first_time = true;
            free(itr);
            return 0;
        }
    }
    
    int ret = UNSUPPORTED;
    /* init private data */
    *priv_data = calloc(1, sizeof(wlr_gamma_priv));
    wlr_gamma_priv *priv = (wlr_gamma_priv *)*priv_data;
    if (!priv) {
        return -ENOMEM;
    }
    
    wl_list_init(&priv->outputs);
    priv->registry = wl_display_get_registry(display);
    wl_registry_add_listener(priv->registry, &registry_listener, priv);
    wl_display_roundtrip(display);
    
    if (priv->gamma_control_manager == NULL) {
        fprintf(stderr, "compositor doesn't support wlr-gamma-control-unstable-v1\n");
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    
    struct output *output;
    wl_list_for_each(output, &priv->outputs, link) {
        output->gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(
            priv->gamma_control_manager, output->wl_output);
        if (output->gamma_control) {
            zwlr_gamma_control_v1_add_listener(output->gamma_control,
                                               &gamma_control_listener, output);
        } else {
            fprintf(stderr, "failed to receive gamma control manager\n");
            goto err;
        }
    }
    wl_display_roundtrip(display);
    
    /* Check that all outputs were init correctly */
    wl_list_for_each(output, &priv->outputs, link) {
        if (output->wl_output == NULL || output->table == NULL) {
            fprintf(stderr, "failed to create gamma table\n");
            goto err;
        }
    }
    
    priv->dpy = display;
    return 0;
    
err:
    dtor(priv);
    return ret;
}

static int set(void *priv_data, const int temp) {
    wlr_gamma_priv *priv = (wlr_gamma_priv *)priv_data;
    
    struct output *output;
    wl_list_for_each(output, &priv->outputs, link) {
        lseek(output->table_fd, 0, SEEK_SET);
        uint16_t *r = output->table;
        uint16_t *g = output->table + output->ramp_size;
        uint16_t *b = output->table + 2 * output->ramp_size;
        fill_gamma_table(r, g, b, 1.0, output->ramp_size, temp);
        zwlr_gamma_control_v1_set_gamma(output->gamma_control,
                                        output->table_fd);
    }
    wl_display_flush(priv->dpy);
    // Register this fd and listen on events
    if (!priv->not_first_time) {
        m_register_fd(wl_display_get_fd(priv->dpy), false, priv);
        stack_push(clients, priv);
    }
    return 0;
}

static int get(void *priv_data) {
    // Unsupported ?
    // Ok anyway, just return 0;
    // It will be supported one day hopefully
    return 0;
}

static void dtor(void *priv_data) {
    if (!leaving) {
        /* Check if we already have a running client for this display */
        for (stack_itr_t *itr = stack_itr_new(clients); itr; itr = stack_itr_next(itr)) {
            wlr_gamma_priv *p = (wlr_gamma_priv *)stack_itr_get_data(itr);
            if (p == priv_data) {
                // do not remove this client as we need to keep it alive;
                free(itr);
                return;
            }
        }
    }
    
    wlr_gamma_priv *priv = (wlr_gamma_priv *)priv_data;
    if (priv->dpy) {
        m_deregister_fd(wl_display_get_fd(priv->dpy));
    }
    struct output *output, *tmp_output;
    wl_list_for_each_safe(output, tmp_output, &priv->outputs, link) {
        wl_list_remove(&output->link);
        destroy_output(output);
    }
    if (priv->registry) {
        wl_registry_destroy(priv->registry);
    }
    if (priv->gamma_control_manager) {
        zwlr_gamma_control_manager_v1_destroy(priv->gamma_control_manager);
    }
    free(priv_data);
    // NOTE: dpy is disconnected on program exit to workaround
    // gamma protocol limitation that resets gamma as soon as display is disconnected.
    // See wl_utils.c
}

static int create_gamma_table(uint32_t ramp_size, uint16_t **table) {
    size_t table_size = ramp_size * 3 * sizeof(uint16_t);
    int fd = create_anonymous_file(table_size, "clightd-gamma-wlr");
    if (fd < 0) {
        fprintf(stderr, "failed to create anonymous file\n");
        return -1;
    }

    void *data = mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "failed to mmap()\n");
        close(fd);
        return -1;
    }

    *table = data;
    return fd;
}

static void destroy_output(struct output *output) {
    size_t table_size = output->ramp_size * 3 * sizeof(uint16_t);
    if (output->table) {
        munmap(output->table, table_size);
    }
    if (output->table_fd != -1) {
        close(output->table_fd);
    }
    if (output->wl_output) {
        wl_output_destroy(output->wl_output);
    }
    if (output->gamma_control) {
        zwlr_gamma_control_v1_destroy(output->gamma_control);
    }
    free(output);
}

static void gamma_control_handle_gamma_size(void *data, 
                                            struct zwlr_gamma_control_v1 *gamma_control, uint32_t ramp_size) {
    struct output *output = data;
    output->ramp_size = ramp_size;
    output->table_fd = create_gamma_table(ramp_size, &output->table);
}

static void gamma_control_handle_failed(void *data,
                                        struct zwlr_gamma_control_v1 *gamma_control) {
    fprintf(stderr, "failed to set gamma table\n");
}

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    wlr_gamma_priv *priv = (wlr_gamma_priv *)data;
    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output *output = calloc(1, sizeof(struct output));
        output->wl_output = wl_registry_bind(registry, name,
            &wl_output_interface, 1);
        wl_list_insert(&priv->outputs, &output->link);
    } else if (strcmp(interface, zwlr_gamma_control_manager_v1_interface.name) == 0) {
        
        priv->gamma_control_manager = wl_registry_bind(registry, name, &zwlr_gamma_control_manager_v1_interface, 1);
    }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry, uint32_t name) {
    
}
