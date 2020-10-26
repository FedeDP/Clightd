#ifdef GAMMA_PRESENT

#include <commons.h>
#include "utils.h"
#include "drm_utils.h"

static int drm_dtor(gamma_client *cl);
static int drm_set_gamma(gamma_client *cl, const int temp);
static int drm_get_gamma(gamma_client *cl);

typedef struct {
    int fd;
    drmModeRes *res;
} drm_gamma_priv;

int drm_get_handler(gamma_client *cl) {
    int ret = WRONG_PLUGIN;
    int fd = drm_open_card(cl->display);
    if (fd < 0) {
        return ret;
    }
    
    drmModeRes *res = drmModeGetResources(fd);
    if (res && res->count_crtcs > 0) {
        cl->handler.priv = malloc(sizeof(drm_gamma_priv));
        drm_gamma_priv *priv = (drm_gamma_priv *)cl->handler.priv;
        if (priv) {
            priv->fd = fd;
            priv->res = res;
            
            cl->handler.dtor = drm_dtor;
            cl->handler.set = drm_set_gamma;
            cl->handler.get = drm_get_gamma;
            ret = 0;
        } else {
            ret = -ENOMEM;
        }
    } else {
        ret = UNSUPPORTED;
    }

    if (ret != 0) {
        if (res) {
            drmModeFreeResources(res);
        }
        if (fd != -1) {
            close(fd);
        }
    }
    return ret;
}

static int drm_dtor(gamma_client *cl) {
     drm_gamma_priv *priv = (drm_gamma_priv *)cl->handler.priv;
     drmModeFreeResources(priv->res);
     return close(priv->fd);
}

static int drm_set_gamma(gamma_client *cl, const int temp) {   
    drm_gamma_priv *priv = (drm_gamma_priv *)cl->handler.priv;
        
    int ret = 0;
    if (!drmIsMaster(priv->fd) && drmSetMaster(priv->fd)) {
        perror("SetMaster");
        ret = -errno;
        goto end;
    }
        
    const double red = get_red(temp) / (double)255;
    const double green = get_green(temp) / (double)255;
    const double blue = get_blue(temp) / (double)255;
    
    for (int i = 0; i < priv->res->count_crtcs && !ret; i++) {
        int id = priv->res->crtcs[i];
        drmModeCrtc *crtc_info = drmModeGetCrtc(priv->fd, id);
        int ramp_size = crtc_info->gamma_size;

        uint16_t *r_gamma = calloc(ramp_size, sizeof(uint16_t));
        uint16_t *g_gamma = calloc(ramp_size, sizeof(uint16_t));
        uint16_t *b_gamma = calloc(ramp_size, sizeof(uint16_t));
        for (int j = 0; j < ramp_size; j++) {
            const double g = 65535.0 * j / ramp_size;
            r_gamma[j] = g * red;
            g_gamma[j] = g * green;
            b_gamma[j] = g * blue;
        }
        int r = drmModeCrtcSetGamma(priv->fd, id, ramp_size, r_gamma, g_gamma, b_gamma);
        if (r) {
            ret = -errno;
            perror("drmModeCrtcSetGamma");
        }
        free(r_gamma);
        free(g_gamma);
        free(b_gamma);
        drmModeFreeCrtc(crtc_info);
    }

    if (drmDropMaster(priv->fd)) {
        perror("DropMaster");
    }

end:
    return ret;
}

static int drm_get_gamma(gamma_client *cl) {
    drm_gamma_priv *priv = (drm_gamma_priv *)cl->handler.priv;
    
    int temp = -1;
    
    int id = priv->res->crtcs[0];
    drmModeCrtc *crtc_info = drmModeGetCrtc(priv->fd, id);
    int ramp_size = crtc_info->gamma_size;
        
    uint16_t *red = calloc(ramp_size, sizeof(uint16_t));
    uint16_t *green = calloc(ramp_size, sizeof(uint16_t));
    uint16_t *blue = calloc(ramp_size, sizeof(uint16_t));
        
    int r = drmModeCrtcGetGamma(priv->fd, id, ramp_size, red, green, blue);
    if (r) {
        perror("drmModeCrtcSetGamma");
    } else {
        temp = get_temp(clamp(red[1], 0, 255), clamp(blue[1], 0, 255));
    }
        
    free(red);
    free(green);
    free(blue);
        
    drmModeFreeCrtc(crtc_info);
    return temp;
}

#endif
