#include "output-management-client-protocol.h"
#include "outputdevice-client-protocol.h"
#include "wl_utils.h"
#include "gamma.h"
#include <module/queue.h>

#define UNIMPLEMENTED(registry_listener) registry_listener {}

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version);
UNIMPLEMENTED(static void registry_handle_global_remove(void *data,
                                                        struct wl_registry *registry, uint32_t name))
static void gamma_applied(void *data, struct org_kde_kwin_outputconfiguration *org_kde_kwin_outputconfiguration);
static void gamma_failed(void *data, struct org_kde_kwin_outputconfiguration *org_kde_kwin_outputconfiguration);

static void device_colorcurves(void *data, struct org_kde_kwin_outputdevice *org_kde_kwin_outputdevice,
                        struct wl_array *red,
                        struct wl_array *green,
                        struct wl_array *blue);
static void device_uuid(void *data, struct org_kde_kwin_outputdevice *org_kde_kwin_outputdevice, const char *uuid);

typedef struct {
    struct org_kde_kwin_outputdevice *od;
    struct wl_array red;
    struct wl_array green;
    struct wl_array blue;
    char *name;
} kwin_color_dev;

typedef struct {
    struct wl_display *dpy;
    queue_t *devices;
    struct org_kde_kwin_outputconfiguration *output_config;
    struct org_kde_kwin_outputmanagement *output_control_manager;
    struct wl_registry *gamma_registry;
    bool applied;
    bool failed;
} kwin_gamma_priv;

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static const struct org_kde_kwin_outputconfiguration_listener gamma_listener = {
    .applied = gamma_applied,
    .failed = gamma_failed,
};

static const struct org_kde_kwin_outputdevice_listener device_listener = {
    .colorcurves = device_colorcurves,
    .uuid = device_uuid,
};

GAMMA("KWin_wl");

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    kwin_gamma_priv *priv = (kwin_gamma_priv *)data;
    if (strcmp(interface, org_kde_kwin_outputdevice_interface.name) == 0) {
        struct org_kde_kwin_outputdevice *od = wl_registry_bind(registry, name, &org_kde_kwin_outputdevice_interface, 1);
        kwin_color_dev *dev = calloc(1, sizeof(kwin_color_dev));
        dev->od = od;
        queue_enqueue(priv->devices, dev);
    } else if (strcmp(interface, org_kde_kwin_outputmanagement_interface.name) == 0) {
        priv->output_control_manager = wl_registry_bind(registry, name, &org_kde_kwin_outputmanagement_interface, 1);
    }
}

static void gamma_applied(void *data, struct org_kde_kwin_outputconfiguration *org_kde_kwin_outputconfiguration) {
    kwin_gamma_priv *priv = (kwin_gamma_priv *)data;
    priv->applied = true;
}

static void gamma_failed(void *data, struct org_kde_kwin_outputconfiguration *org_kde_kwin_outputconfiguration) {
    kwin_gamma_priv *priv = (kwin_gamma_priv *)data;
    priv->failed = true;
}

static void device_colorcurves(void *data, struct org_kde_kwin_outputdevice *org_kde_kwin_outputdevice,
                        struct wl_array *red,
                        struct wl_array *green,
                        struct wl_array *blue)
{
    kwin_color_dev *priv = (kwin_color_dev *)data;
    wl_array_init(&priv->red);
    wl_array_copy(&priv->red, red);
    
    wl_array_init(&priv->green);
    wl_array_copy(&priv->green, green);

    wl_array_init(&priv->blue);
    wl_array_copy(&priv->blue, blue);
}

static void device_uuid(void *data, struct org_kde_kwin_outputdevice *org_kde_kwin_outputdevice, const char *uuid) {
    kwin_color_dev *priv = (kwin_color_dev *)data;
    priv->name = strdup(uuid);
}

static void device_dtor(void *dev) {
    kwin_color_dev *priv = (kwin_color_dev *)dev;
    org_kde_kwin_outputdevice_destroy(priv->od);
    wl_array_release(&priv->red);
    wl_array_release(&priv->green);
    wl_array_release(&priv->blue);
    free(priv->name);
}

static int validate(const char **id, const char *env,  void **priv_data) {
    int ret = 0;
    struct wl_display *dpy = fetch_wl_display(*id, env);
    if (dpy == NULL) {
        ret = WRONG_PLUGIN;
        return ret;
    }
    
    *priv_data = calloc(1, sizeof(kwin_gamma_priv));
    kwin_gamma_priv *priv = (kwin_gamma_priv *)*priv_data;
    if (!priv) {
        return -ENOMEM;
    }
    
    priv->dpy = dpy;
    priv->devices = queue_new(device_dtor);
    priv->gamma_registry = wl_display_get_registry(priv->dpy);
    wl_registry_add_listener(priv->gamma_registry, &registry_listener, priv);
    wl_display_roundtrip(priv->dpy);
    if (priv->output_control_manager == NULL) {
        fprintf(stderr, "compositor doesn't support '%s'\n", org_kde_kwin_outputmanagement_interface.name);
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    
    priv->output_config = org_kde_kwin_outputmanagement_create_configuration(priv->output_control_manager);
    if (priv->output_config) {
        org_kde_kwin_outputconfiguration_add_listener(priv->output_config, &gamma_listener, priv);
    } else {
        fprintf(stderr, "failed to receive KWin output management configuration\n");
        ret = -errno;
        goto err;
    }
    wl_display_roundtrip(priv->dpy);
    
    for (queue_itr_t *itr = queue_itr_new(priv->devices); itr; itr = queue_itr_next(itr)) {
        kwin_color_dev *dev = queue_itr_get_data(itr);
        org_kde_kwin_outputdevice_add_listener(dev->od, &device_listener, dev);
    }
    wl_display_roundtrip(priv->dpy);
    
    /* No supported output found */
    if (queue_length(priv->devices) == 0) {
        ret = UNSUPPORTED;
    }
        
err:
    if (ret != 0) {
        dtor(priv);
    }
    return ret;
}

static int set(void *priv_data, const int temp) {
    kwin_gamma_priv *priv = (kwin_gamma_priv *)priv_data;
    for (queue_itr_t *itr = queue_itr_new(priv->devices); itr; itr = queue_itr_next(itr)) {
        kwin_color_dev *dev = queue_itr_get_data(itr);
        const double br = get_gamma_brightness(dev->name);
        uint16_t *r = (uint16_t *)dev->red.data;
        uint16_t *g = (uint16_t *)dev->green.data;
        uint16_t *b = (uint16_t *)dev->blue.data;
        fill_gamma_table(r, g, b, br, dev->red.size, temp);
        org_kde_kwin_outputconfiguration_colorcurves(priv->output_config, dev->od, &dev->red, &dev->green, &dev->blue);
    }
    return 0;
}

static int get(void *priv_data) {
    kwin_gamma_priv *priv = (kwin_gamma_priv *)priv_data;
    kwin_color_dev *dev = queue_peek(priv->devices);
    const double br = get_gamma_brightness(dev->name);
    uint16_t *r = (uint16_t *)dev->red.data;
    uint16_t *b = (uint16_t *)dev->blue.data;
    return get_temp(clamp(r[1] / br, 0, 255), clamp(b[1] / br, 0, 255));;
}

static void dtor(void *priv_data) {
    kwin_gamma_priv *priv = (kwin_gamma_priv *)priv_data;
    queue_free(priv->devices);
    if (priv->gamma_registry) {
        wl_registry_destroy(priv->gamma_registry);
    }
    if (priv->output_config) {
        org_kde_kwin_outputconfiguration_destroy(priv->output_config);
    }
    if (priv->output_control_manager) {
        org_kde_kwin_outputmanagement_destroy(priv->output_control_manager);
    }
    free(priv_data);
    // NOTE: dpy is disconnected on program exit to workaround
    // gamma protocol limitation that resets gamma as soon as display is disconnected.
    // See wl_utils.c
}
                                                                        
