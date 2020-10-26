#ifdef GAMMA_PRESENT

#include <wayland-client.h>
#include <sys/mman.h>
#include "../build/wlr-gamma-control-unstable-v1-client-protocol.h"
#include "commons.h"
#include "utils.h"

#define WLR_TEMP_MAX 10000

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
} wlr_gamma_priv;

static int wl_set_gamma(gamma_client *cl, const int temp);
static int wl_get_gamma(gamma_client *cl);
static int wl_dtor(gamma_client *cl);
static int create_anonymous_file(off_t size);
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
static void fill_gamma_table(uint16_t *table, uint32_t ramp_size, double gamma);

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
    .gamma_size = gamma_control_handle_gamma_size,
    .failed = gamma_control_handle_failed,
};

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static int create_anonymous_file(off_t size) {
    char template[] = "/tmp/clightd-gamma-wlr-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) {
        return -1;
    }

    int ret;
    do {
        errno = 0;
        ret = ftruncate(fd, size);
    } while (errno == EINTR);
    if (ret < 0) {
        close(fd);
        return -1;
    }

    unlink(template);
    return fd;
}

static int create_gamma_table(uint32_t ramp_size, uint16_t **table) {
    size_t table_size = ramp_size * 3 * sizeof(uint16_t);
    int fd = create_anonymous_file(table_size);
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
    munmap(output->table, table_size);
    close(output->table_fd);
      // TODO?? output->wl_output
    zwlr_gamma_control_v1_destroy(output->gamma_control);
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

static void fill_gamma_table(uint16_t *table, uint32_t ramp_size, double gamma) {
    uint16_t *r = table;
    uint16_t *g = table + ramp_size;
    uint16_t *b = table + 2 * ramp_size;
    for (uint32_t i = 0; i < ramp_size; ++i) {
        double val = (double)i / (ramp_size - 1);
        val = pow(val, 1.0 / gamma);
        val = clamp(val, 0.0, 1.0);
        r[i] = g[i] = b[i] = (uint16_t)(UINT16_MAX * val);
    }
}

int wl_get_handler(gamma_client *cl) {
    struct wl_display *display = wl_display_connect(cl->display);
    if (display == NULL) {
        return WRONG_PLUGIN;
    }

    /* init private data */
    cl->handler.priv = calloc(1, sizeof(wlr_gamma_priv));
    wlr_gamma_priv *priv = (wlr_gamma_priv *)cl->handler.priv;
    if (!priv) {
        wl_display_disconnect(display);
        return -ENOMEM;
    }
    
    priv->registry = wl_display_get_registry(display);
    wl_registry_add_listener(priv->registry, &registry_listener, priv);
    wl_display_roundtrip(display);

    if (priv->gamma_control_manager == NULL) {
        fprintf(stderr, "compositor doesn't support wlr-gamma-control-unstable-v1\n");
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
    cl->handler.set = wl_set_gamma;
    cl->handler.get = wl_get_gamma;
    cl->handler.dtor = wl_dtor;
    
    return 0;

err:
    wl_dtor(cl);
    return UNSUPPORTED;
}

static int wl_dtor(gamma_client *cl) {
    wlr_gamma_priv *priv = (wlr_gamma_priv *)cl->handler.priv;
    struct output *output;
    wl_list_for_each(output, &priv->outputs, link) {
        wl_list_remove(&output->link);
        destroy_output(output);
    }
    wl_registry_destroy(priv->registry);
    zwlr_gamma_control_manager_v1_destroy(priv->gamma_control_manager);
    wl_display_disconnect(priv->dpy);
    return 0;
}

static int wl_set_gamma(gamma_client *cl, const int temp) {
    wlr_gamma_priv *priv = (wlr_gamma_priv *)cl->handler.priv;

    struct output *output;
    wl_list_for_each(output, &priv->outputs, link) {
        fill_gamma_table(output->table, output->ramp_size, (double)temp / WLR_TEMP_MAX);
        zwlr_gamma_control_v1_set_gamma(output->gamma_control,
            output->table_fd);
    }

    while (wl_display_dispatch(priv->dpy) != -1) {
        // This space is intentionnally left blank
    }
    return 0;
}

static int wl_get_gamma(gamma_client *cl) {
    // Unsupported ?
    return -1;
}

#endif
