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
    
    char drm_name[32];
    for (int i = 0; i < priv->res->count_connectors && !ret; i++) {
        drmModeConnectorPtr p = drmModeGetConnector(priv->fd, priv->res->connectors[i]);
        if (!p || p->connection != DRM_MODE_CONNECTED) {
            if (p) {
                drmModeFreeConnector(p);
            }
            continue;
        }

        const char *conn_type_name = drmModeGetConnectorTypeName(p->connector_type);
        snprintf(drm_name, sizeof(drm_name), "%s-%u", conn_type_name, p->connector_type_id);
        const double br = fetch_gamma_brightness(drm_name);
        for (int j = 0; j < p->count_encoders; j++) {
            drmModeEncoderPtr enc = drmModeGetEncoder(priv->fd, p->encoders[j]);
            if (!enc) {
                continue;
            }
            drmModeCrtc *crtc_info = drmModeGetCrtc(priv->fd, enc->crtc_id);
            if (!crtc_info) {
                drmModeFreeEncoder(enc);
                continue;
            }
            int ramp_size = crtc_info->gamma_size;
            uint16_t *r = calloc(ramp_size, sizeof(uint16_t));
            uint16_t *g = calloc(ramp_size, sizeof(uint16_t));
            uint16_t *b = calloc(ramp_size, sizeof(uint16_t));
            fill_gamma_table(r, g, b, br, ramp_size, temp);
            ret = drmModeCrtcSetGamma(priv->fd, enc->crtc_id, ramp_size, r, g, b);
            if (ret) {
                ret = -errno;
                perror("drmModeCrtcSetGamma");
            }
            free(r);
            free(g);
            free(b);
            drmModeFreeCrtc(crtc_info);
            drmModeFreeEncoder(enc);
        }
        drmModeFreeConnector(p);
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
    
    char drm_name[32];
    for (int i = 0; i < priv->res->count_connectors; i++) {
        drmModeConnectorPtr p = drmModeGetConnector(priv->fd, priv->res->connectors[i]);
        if (!p || (p->connection != DRM_MODE_CONNECTED && i < priv->res->count_connectors - 1)) {
            // Skip output not connected if there is any others
            if (p) {
                drmModeFreeConnector(p);
            }
            continue;
        }
        const char *conn_type_name = drmModeGetConnectorTypeName(p->connector_type);
        snprintf(drm_name, sizeof(drm_name), "%s-%u", conn_type_name, p->connector_type_id);
        const double br = fetch_gamma_brightness(drm_name);
        drmModeEncoderPtr enc = drmModeGetEncoder(priv->fd, p->encoders[0]);
        if (!enc) {
            drmModeFreeConnector(p);
            continue;
        }
        drmModeCrtc *crtc_info = drmModeGetCrtc(priv->fd, enc->crtc_id);
        int ramp_size = crtc_info->gamma_size;
        
        uint16_t *red = calloc(ramp_size, sizeof(uint16_t));
        uint16_t *green = calloc(ramp_size, sizeof(uint16_t));
        uint16_t *blue = calloc(ramp_size, sizeof(uint16_t));
        
        int r = drmModeCrtcGetGamma(priv->fd, enc->crtc_id, ramp_size, red, green, blue);
        if (r) {
            perror("drmModeCrtcSetGamma");
        } else {
            temp = get_temp(clamp(red[1] / br, 0, 255), clamp(blue[1] / br, 0, 255));
        }
        
        free(red);
        free(green);
        free(blue);
        
        drmModeFreeCrtc(crtc_info);
        drmModeFreeEncoder(enc);
        drmModeFreeConnector(p);
        break;
    }
    return temp;
}

static void dtor(void *priv_data) {
    drm_gamma_priv *priv = (drm_gamma_priv *)priv_data;
    drmModeFreeResources(priv->res);
    close(priv->fd);
    free(priv_data);
}
