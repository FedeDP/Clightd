#include "gamma.h"
#include <commons.h>
#include <X11/extensions/Xrandr.h>

typedef struct {
    Display *dpy;
    XRRScreenResources *res;
} xorg_gamma_priv;

GAMMA("Xorg");

static int validate(const char **id, const char *env, void **priv_data) {
    int ret = WRONG_PLUGIN;
    
    setenv("XAUTHORITY", env, 1);
    
    Display *dpy = XOpenDisplay(*id);
    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
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
    
    unsetenv("XAUTHORITY");
    return ret;
}

static int set(void *priv_data, const int temp) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)priv_data;
    
    for (int i = 0; i < priv->res->ncrtc; i++) {
        const int crtcxid = priv->res->crtcs[i];
        const int size = XRRGetCrtcGammaSize(priv->dpy, crtcxid);
        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);
        fill_gamma_table(crtc_gamma->red, crtc_gamma->green, crtc_gamma->blue, size, temp);
        XRRSetCrtcGamma(priv->dpy, crtcxid, crtc_gamma);
        XFree(crtc_gamma);
    }
    return 0;
}

static int get(void *priv_data) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)priv_data;
    
    int temp = -1;
    if (priv->res && priv->res->ncrtc > 0) {
        XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(priv->dpy, priv->res->crtcs[0]);
        const int size = crtc_gamma->size;
        const int g = (65535.0 * (size - 1) / size) / 255;
        temp = get_temp(clamp(crtc_gamma->red[size - 1] / g, 0, 255), clamp(crtc_gamma->blue[size - 1] / g, 0, 255));
        XFree(crtc_gamma);
    }
    return temp;
}

static void dtor(void *priv_data) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)priv_data;
    XRRFreeScreenResources(priv->res);
    XCloseDisplay(priv->dpy);
    free(priv_data);
}
