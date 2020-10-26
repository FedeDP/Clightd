#ifdef DPMS_PRESENT

#include "commons.h"
#include "drm_utils.h"
 
static drmModeConnectorPtr get_active_connector(int fd, int connector_id);
static drmModePropertyPtr drm_get_prop(int fd, drmModeConnectorPtr connector, const char *name);

/*
 * state will be one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 * 
 * Clightd returns -1 if dpms is disabled
 */
int drm_get_dpms_state(const char *card) {
    int state = -1;
    drmModeConnectorPtr connector;
    
    int fd = drm_open_card(card);
    if (fd < 0) {
        return WRONG_PLUGIN;
    }
    
    drmModeRes *res = drmModeGetResources(fd);
    if (res) {
        for (int i = 0; i < res->count_connectors; i++) {
            connector = get_active_connector(fd, res->connectors[i]);
            if (!connector) {
                continue;
            }
        
            drmModePropertyPtr p = drm_get_prop(fd, connector, "DPMS");
            if (p) {
                /* prop_id is 2, it means it is second prop */
                state = (int)connector->prop_values[p->prop_id - 1];
                drmModeFreeProperty(p);
                drmModeFreeConnector(connector);
                break;
            }
        }
        drmModeFreeResources(res);
    }
    close(fd);
    return state;
}

int drm_set_dpms_state(const char *card, int level) {
    int err = 0;
    drmModeConnectorPtr connector;
    drmModePropertyPtr prop;
    
    int fd = drm_open_card(card);
    if (fd < 0) {
        return WRONG_PLUGIN;
    }
    
    if (!drmIsMaster(fd) && drmSetMaster(fd)) {
        perror("SetMaster");
        err = -errno;
        goto end;
    }
    
    drmModeRes *res = drmModeGetResources(fd);
    if (res) {
        for (int i = 0; i < res->count_connectors; i++) {
            connector = get_active_connector(fd, res->connectors[i]);
            if (!connector) {
                continue;
            }
            
            prop = drm_get_prop(fd, connector, "DPMS");
            if (!prop) {
                drmModeFreeConnector(connector);
                continue;
            }
            if (drmModeConnectorSetProperty(fd, connector->connector_id, prop->prop_id, level)) {
                perror("drmModeConnectorSetProperty");
            }
            drmModeFreeProperty(prop);
            drmModeFreeConnector(connector);
        }
        drmModeFreeResources(res);
    } else {
        err = -errno;
    }
    
    if (drmDropMaster(fd)) {
        perror("DropMaster");
        err = -errno;
    }

end:
    if (fd > 0) {
        close(fd);
    }
    return err;
}

static drmModeConnectorPtr get_active_connector(int fd, int connector_id) {
    drmModeConnectorPtr connector = drmModeGetConnector(fd, connector_id);
    
    if (connector) {
        if (connector->connection == DRM_MODE_CONNECTED 
            && connector->count_modes > 0 && connector->encoder_id != 0) {
            return connector;
            }
            drmModeFreeConnector(connector);
    }
    return NULL;
}

static drmModePropertyPtr drm_get_prop(int fd, drmModeConnectorPtr connector, const char *name) {
    drmModePropertyPtr props;
    
    for (int i = 0; i < connector->count_props; i++) {
        props = drmModeGetProperty(fd, connector->props[i]);
        if (!props) {
            continue;
        }
        if (!strcmp(props->name, name)) {
            return props;
        }
        drmModeFreeProperty(props);
    }
    return NULL;
}

#endif
