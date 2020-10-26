#ifdef GAMMA_PRESENT

#include "xorg.h"
#include "utils.h"
#include <commons.h>
#include <X11/extensions/Xrandr.h>

static int xorg_dtor(gamma_client *cl);
static int xorg_set_gamma(gamma_client *cl, const int temp);
static int xorg_get_gamma(gamma_client *cl);

typedef struct {
    Display *dpy;
    XRRScreenResources *res;
} xorg_gamma_priv;

int xorg_get_handler(gamma_client *cl, const char *xauthority) {
    int ret = WRONG_PLUGIN;
    setenv("XAUTHORITY", xauthority, 1);
    Display *dpy = XOpenDisplay(cl->display);
    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
        if (res) {
            cl->handler.priv = calloc(1, sizeof(xorg_gamma_priv));
            xorg_gamma_priv *priv = (xorg_gamma_priv *)cl->handler.priv;
            priv->dpy = dpy;
            priv->res = res;
    
            cl->handler.dtor = xorg_dtor;
            cl->handler.set = xorg_set_gamma;
            cl->handler.get = xorg_get_gamma;
            ret = 0;
        } else {
            ret = UNSUPPORTED;
            XCloseDisplay(dpy);
        }
    }
    unsetenv("XAUTHORITY");
    return ret;
}

static int xorg_dtor(gamma_client *cl) { 
    xorg_gamma_priv *priv = (xorg_gamma_priv *)cl->handler.priv;
    XRRFreeScreenResources(priv->res);
    return XCloseDisplay(priv->dpy);
}

static int xorg_set_gamma(gamma_client *cl, const int temp) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)cl->handler.priv;
        
    double red = get_red(temp) / (double)255;
    double green = get_green(temp) / (double)255;
    double blue = get_blue(temp) / (double)255;
            
    for (int i = 0; i < priv->res->ncrtc; i++) {
        const int crtcxid = priv->res->crtcs[i];
        const int size = XRRGetCrtcGammaSize(priv->dpy, crtcxid);
        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);
        for (int j = 0; j < size; j++) {
            const double g = 65535.0 * j / size;
            crtc_gamma->red[j] = g * red;
            crtc_gamma->green[j] = g * green;
            crtc_gamma->blue[j] = g * blue;
        }
        XRRSetCrtcGamma(priv->dpy, crtcxid, crtc_gamma);
        XFree(crtc_gamma);
    }
    return 0;
}

static int xorg_get_gamma(gamma_client *cl) {
    xorg_gamma_priv *priv = (xorg_gamma_priv *)cl->handler.priv;
         
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

#endif
