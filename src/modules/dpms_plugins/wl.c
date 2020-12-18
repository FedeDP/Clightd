#include "org_kde_kwin_dpms-client-protocol.h"
#include "wl_utils.h"
#include "dpms.h"

struct output {
    struct wl_output *wl_output;
    struct org_kde_kwin_dpms *dpms_control;
    uint32_t supported;
    uint32_t mode;
    bool done;
    struct wl_list link;
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version);
static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry, uint32_t name);
static void dpms_control_handle_supported(void *data, struct org_kde_kwin_dpms *org_kde_kwin_dpms, uint32_t supported);
static void dpms_control_handle_mode(void *data, struct org_kde_kwin_dpms *org_kde_kwin_dpms, uint32_t mode);
static void dpms_control_handle_done(void *data, struct org_kde_kwin_dpms *org_kde_kwin_dpms);

static int wl_init(const char *display, const char *env);
static void wl_deinit(void);
static void destroy_node(struct output *output);

static struct wl_display *dpy;
static struct wl_list outputs;
static struct org_kde_kwin_dpms_manager *dpms_control_manager;
static struct wl_registry *dpms_registry;

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static const struct org_kde_kwin_dpms_listener dpms_listener = {
    .supported = dpms_control_handle_supported,
    .mode = dpms_control_handle_mode,
    .done = dpms_control_handle_done,
};

DPMS("Wl");

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output *output = calloc(1, sizeof(struct output));
        output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        wl_list_insert(&outputs, &output->link);
    } else if (strcmp(interface, org_kde_kwin_dpms_manager_interface.name) == 0) {
        dpms_control_manager = wl_registry_bind(registry, name, &org_kde_kwin_dpms_manager_interface, 1);
    }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry, uint32_t name) {
    
}

static void dpms_control_handle_supported(void *data, struct org_kde_kwin_dpms *org_kde_kwin_dpms, uint32_t supported) {
     struct output *output = data;
     output->supported = supported;
}

static void dpms_control_handle_mode(void *data, struct org_kde_kwin_dpms *org_kde_kwin_dpms, uint32_t mode) {
    struct output *output = data;
     output->mode = mode;
}

static void dpms_control_handle_done(void *data, struct org_kde_kwin_dpms *org_kde_kwin_dpms) {
    struct output *output = data;
    output->done = true;
}

static int wl_init(const char *display, const char *env) {
    int ret = 0;
    struct wl_display *dpy = fetch_wl_display(display, env);
    if (dpy == NULL) {
        ret = WRONG_PLUGIN;
        return ret;
    }
    
    wl_list_init(&outputs);
    dpms_registry = wl_display_get_registry(dpy);
    wl_registry_add_listener(dpms_registry, &registry_listener, NULL);
    wl_display_roundtrip(dpy);

     if (dpms_control_manager == NULL) {
        fprintf(stderr, "compositor doesn't support org_kde_kwin_dpms\n");
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }

    struct output *output;
    struct output *tmp_output;
    wl_list_for_each(output, &outputs, link) {
        output->dpms_control = org_kde_kwin_dpms_manager_get(dpms_control_manager, output->wl_output);
        if (output->dpms_control) {
            org_kde_kwin_dpms_add_listener(output->dpms_control, &dpms_listener, output);
        } else {
            fprintf(stderr, "failed to receive gamma control manager\n");
            ret = -errno;
            goto err;
        }
    }
    wl_display_roundtrip(dpy);
    
     /* Check that all outputs were inited correctly */
    wl_list_for_each_safe(output, tmp_output, &outputs, link) {
        if (output->wl_output == NULL || output->dpms_control == NULL) {
            fprintf(stderr, "failed to create dpms output\n");
            ret = -ENOMEM;
            break;
        }
        if (!output->supported) {
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
        destroy_node(output);
    }
    if (dpms_registry) {
        wl_registry_destroy(dpms_registry);
    }
    if (dpms_control_manager) {
        org_kde_kwin_dpms_manager_destroy(dpms_control_manager);
    }
    // NOTE: dpy is disconnected on program exit to workaround
    // gamma protocol limitation that resets gamma as soon as display is disconnected.
    // See wl_utils.c
}

static void destroy_node(struct output *output) {
    wl_list_remove(&output->link);
    org_kde_kwin_dpms_destroy(output->dpms_control);
    wl_output_destroy(output->wl_output);
    free(output);
}

static int get(const char **display, const char *env) {
    int ret = wl_init(*display, env);
    if (ret == 0) {
        struct output *output;
         wl_list_for_each(output, &outputs, link) {
            // Only store first output mode
            ret = output->mode;
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
            org_kde_kwin_dpms_set(output->dpms_control, level);
        }
        wl_display_roundtrip(dpy);
        wl_deinit();
    }
    return ret;
}
