#include "gamma.h"
#include <commons.h>
#include "bus_utils.h"
#include "xorg_utils.h"
#include <X11/extensions/Xrandr.h>

#define XORG_DRM_MAP_ENV          "CLIGHTD_XORG_TO_DRM"

typedef struct {
    Display *dpy;
    XRRScreenResources *res;
} xorg_gamma_priv;

GAMMA("Xorg");

static int validate(const char **id, const char *env, void **priv_data) {
    int ret = WRONG_PLUGIN;
    
    Display *dpy = fetch_xorg_display(id, env);
    if (dpy) {
        Window root = DefaultRootWindow(dpy);
        XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
        if (res) {
            *priv_data = calloc(1, sizeof(xorg_gamma_priv));
            xorg_gamma_priv *priv = (xorg_gamma_priv *)*priv_data;
            priv->dpy = dpy;
            priv->res = res;
            ret = 0;
        } else {
            ret = UNSUPPORTED;
            XCloseDisplay(dpy);
        }
    }
    return ret;
}

static double get_output_br(XRROutputInfo *info) {
    /*
     * Sometimes Xorg output name differs from
     * /sys/class/drm node
     * Eg: drm node is called HDMI-A-1 but xorg sees HDMI-A-0 :/
     */
    const char *to_drm_mapper = getenv(XORG_DRM_MAP_ENV);
    if (to_drm_mapper) {
        char map[1024];
        snprintf(map, sizeof(map), "%s", to_drm_mapper);
        char *s = strtok(map, ",");
        while (s) {
            char *val = strchr(s, ':');
            if (val && strlen(val) > 0) {
                *val = '\0';
                val++;
            } else {
                fprintf(stderr, "Wrong %s format: %s\n", XORG_DRM_MAP_ENV, s);
                goto err;
            }
            if (strcmp(s, info->name) == 0) {
                return get_gamma_brightness(val);
            }
            s = strtok(NULL, ",");
        }
    }

err:
    return get_gamma_brightness(info->name);
}

static int set(void *priv_data, const int temp) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)priv_data;
    
    for (int i = 0; i < priv->res->noutput; i++) {
        XRROutputInfo *info = XRRGetOutputInfo(priv->dpy, priv->res, priv->res->outputs[i]);
        if (!info || info->crtc == 0 || info->connection != RR_Connected) {
            if (info) {
                XRRFreeOutputInfo(info);
            }
            continue;
        }
        const double br = get_output_br(info);
        const int crtcxid = info->crtc;
        const int size = XRRGetCrtcGammaSize(priv->dpy, crtcxid);
        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);
        fill_gamma_table(crtc_gamma->red, crtc_gamma->green, crtc_gamma->blue, br, size, temp);
        XRRSetCrtcGamma(priv->dpy, crtcxid, crtc_gamma);
        XFree(crtc_gamma);
        XRRFreeOutputInfo(info);
    }
    return 0;
}

static int get(void *priv_data) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)priv_data;
    
    int temp = -1;
    if (priv->res && priv->res->ncrtc > 0) {
        for (int i = 0; i < priv->res->noutput; i++) {
            XRROutputInfo *info = XRRGetOutputInfo(priv->dpy, priv->res, priv->res->outputs[i]);
            if (!info || info->crtc == 0 || info->connection != RR_Connected) {
                if (info) {
                    XRRFreeOutputInfo(info);
                }
                continue;
            }
            const double br = get_output_br(info);
            const int crtcxid = info->crtc;
            XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(priv->dpy, crtcxid);
            const int size = crtc_gamma->size;
            const int g = (65535.0 * (size - 1) / size) / 255;
            temp = get_temp(clamp(crtc_gamma->red[size - 1] / (g * br), 0, 255), clamp(crtc_gamma->blue[size - 1] / (g * br), 0, 255));
            XFree(crtc_gamma);
            XRRFreeOutputInfo(info);
            break;
        }
    }
    return temp;
}

static void dtor(void *priv_data) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)priv_data;
    XRRFreeScreenResources(priv->res);
    XCloseDisplay(priv->dpy);
    free(priv_data);
}
