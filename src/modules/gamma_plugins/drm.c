#include <commons.h>
#include "gamma.h"
#include "drm_utils.h"

typedef struct {
    int fd;
    drmModeRes *res;
} drm_gamma_priv;

GAMMA("Drm");

static int validate(const char **id, const char *env, void **priv_data) {
    int ret = WRONG_PLUGIN;
    int fd = drm_open_card(id);
    if (fd < 0) {
        return ret;
    }
    
    drmModeRes *res = drmModeGetResources(fd);
    if (res && res->count_crtcs > 0) {
        *priv_data = malloc(sizeof(drm_gamma_priv));
        drm_gamma_priv *priv = (drm_gamma_priv *)*priv_data;
        if (priv) {
            priv->fd = fd;
            priv->res = res;
            ret = 0;
        } else {
            ret = -ENOMEM;
        }
    } else {
        perror("gamma drmModeGetResources");
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

static int set(void *priv_data, const int temp) {
    drm_gamma_priv *priv = (drm_gamma_priv *)priv_data;
    
    int ret = 0;
    
    if (drmSetMaster(priv->fd)) {
        perror("SetMaster");
        ret = -errno;
        goto end;
    }
    
    for (int i = 0; i < priv->res->count_crtcs && !ret; i++) {
        int id = priv->res->crtcs[i];
        drmModeCrtc *crtc_info = drmModeGetCrtc(priv->fd, id);
        int ramp_size = crtc_info->gamma_size;
        uint16_t *r = calloc(ramp_size, sizeof(uint16_t));
        uint16_t *g = calloc(ramp_size, sizeof(uint16_t));
        uint16_t *b = calloc(ramp_size, sizeof(uint16_t));
        fill_gamma_table(r, g, b, ramp_size, temp);
        ret = drmModeCrtcSetGamma(priv->fd, id, ramp_size, r, g, b);
        if (ret) {
            ret = -errno;
            perror("drmModeCrtcSetGamma");
        }
        free(r);
        free(g);
        free(b);
        drmModeFreeCrtc(crtc_info);
    }
    
    if (drmDropMaster(priv->fd)) {
        perror("DropMaster");
    }
    
    end:
    return ret;
}

static int get(void *priv_data) {
    drm_gamma_priv *priv = (drm_gamma_priv *)priv_data;
    
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

static void dtor(void *priv_data) {
    drm_gamma_priv *priv = (drm_gamma_priv *)priv_data;
    drmModeFreeResources(priv->res);
    close(priv->fd);
    free(priv_data);
}
