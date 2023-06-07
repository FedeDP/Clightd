#include "wlr-output-power-management-unstable-v1-client-protocol.h"
#include "wl_utils.h"
#include "dpms.h"

struct output {
    struct wl_output *wl_output;
    struct zwlr_output_power_v1 *dpms_control;
    uint32_t mode;
    bool failed;
    struct wl_list link;
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version);
static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry, uint32_t name);
static void dpms_control_handle_mode(void *data, struct zwlr_output_power_v1 *zwlr_output_power_v1, uint32_t mode);
static void dpms_control_handle_failed(void *data, struct zwlr_output_power_v1 *zwlr_output_power_v1);

static int wl_init(const char *display, const char *env);
static void wl_deinit(void);
static void destroy_node(struct output *output);

static struct wl_display *dpy;
static struct wl_list outputs;
static struct zwlr_output_power_manager_v1 *dpms_control_manager;
static struct wl_registry *dpms_registry;

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static const struct zwlr_output_power_v1_listener dpms_listener = {
    .failed = dpms_control_handle_failed,
    .mode = dpms_control_handle_mode,
};

DPMS("Wl");

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output *output = calloc(1, sizeof(struct output));
        output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        wl_list_insert(&outputs, &output->link);
    } else if (strcmp(interface, zwlr_output_power_manager_v1_interface.name) == 0) {
        dpms_control_manager = wl_registry_bind(registry, name, &zwlr_output_power_manager_v1_interface, 1);
    }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry, uint32_t name) {
    
}

static void dpms_control_handle_mode(void *data, struct zwlr_output_power_v1 *zwlr_output_power_v1, uint32_t mode) {
    struct output *output = data;
    output->mode = mode;
}

static void dpms_control_handle_failed(void *data, struct zwlr_output_power_v1 *zwlr_output_power_v1) {
    struct output *output = data;
    output->failed = true;
}

static int wl_init(const char *display, const char *env) {
    int ret = 0;
    dpy = fetch_wl_display(display, env);
    if (dpy == NULL) {
        ret = WRONG_PLUGIN;
        return ret;
    }
    wl_list_init(&outputs);
    dpms_registry = wl_display_get_registry(dpy);
    wl_registry_add_listener(dpms_registry, &registry_listener, NULL);
    wl_display_roundtrip(dpy);

     if (dpms_control_manager == NULL) {
        fprintf(stderr, "compositor doesn't support wlr-output-power-management-unstable-v1\n");
        ret = /*COMPOSITOR_NO_PROTOCOL*/ WRONG_PLUGIN; // Since we want DPMS to try kwin_wl afterwards
        goto err;
    }

    struct output *output;
    struct output *tmp_output;
    wl_list_for_each(output, &outputs, link) {
        output->dpms_control = zwlr_output_power_manager_v1_get_output_power(dpms_control_manager, output->wl_output);
        if (output->dpms_control) {
            zwlr_output_power_v1_add_listener(output->dpms_control, &dpms_listener, output);
        } else {
            fprintf(stderr, "failed to receive wlr DPMS control manager\n");
            ret = -errno;
            goto err;
        }
    }
    wl_display_roundtrip(dpy);
    
     /* Check that all outputs were inited correctly */
    wl_list_for_each_safe(output, tmp_output, &outputs, link) {
        if (output->wl_output == NULL || output->dpms_control == NULL) {
            fprintf(stderr, "failed to create wlr DPMS output\n");
            ret = -ENOMEM;
            break;
        }
        if (output->failed) {
            wl_list_remove(&output->link);
            destroy_node(output);
        }
    }
    
    /* No supported output found */
    if (wl_list_length(&outputs) == 0) {
        ret = UNSUPPORTED;
    }
    
err:
    if (ret != 0) {
        wl_deinit();
    }
    return ret;
}

static void wl_deinit(void) {
    struct output *output;
    struct output *tmp_output;
    wl_list_for_each_safe(output, tmp_output, &outputs, link) {
        wl_list_remove(&output->link);
        destroy_node(output);
    }
    if (dpms_registry) {
        wl_registry_destroy(dpms_registry);
    }
    if (dpms_control_manager) {
        zwlr_output_power_manager_v1_destroy(dpms_control_manager);
    }
    // NOTE: dpy is disconnected on program exit to workaround
    // gamma protocol limitation that resets gamma as soon as display is disconnected.
    // See wl_utils.c
}

static void destroy_node(struct output *output) {
    if (output->wl_output) {
        wl_output_destroy(output->wl_output);
    }
    if (output->dpms_control) {
        zwlr_output_power_v1_destroy(output->dpms_control);
    }
    free(output);
}

static int get(const char **display, const char *env) {
    int ret = wl_init(*display, env);
    if (ret == 0) {
        struct output *output;
         wl_list_for_each(output, &outputs, link) {
            // Only store first output mode
            // Negate the return because Clightd returns value
            // is 0 ON, 1 OFF.
            // See zwlr_output_power_v1_mode
            ret = !output->mode;
            break;
        }
        wl_deinit();
    }
    return ret;
}

static int set(const char **display, const char *env, int level) {
    int ret = wl_init(*display, env);
    if (ret == 0) {
        struct output *output;
        wl_list_for_each(output, &outputs, link) {
            // Negate the level because Clightd uses value
            // is 0 ON, 1 OFF.
            // See zwlr_output_power_v1_mode
            zwlr_output_power_v1_set_mode(output->dpms_control, !level);
        }
        wl_display_roundtrip(dpy);
        wl_deinit();
    }
    return ret;
}
