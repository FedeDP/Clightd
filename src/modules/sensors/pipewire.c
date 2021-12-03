#ifdef PIPEWIRE_PRESENT

#include "camera.h"
#include "bus_utils.h"
#include <pwd.h>
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/utils/result.h>
#include <pipewire/pipewire.h>

#define PW_NAME                 "Pipewire"
#define BUILD_KEY(id) \
    char key[20]; \
    snprintf(key, sizeof(key), "Node%d", id);

typedef struct {
    uint32_t id;
    struct spa_hook proxy_listener;
    struct pw_proxy *proxy;
    const char *action;
} pw_node_t;

typedef struct {
    double *pct;
    int capture_idx;
    bool with_err;
    uint32_t width; // real width, can be cropped
    uint32_t height; // real height, can be cropped
} capture_settings_t;

typedef struct {
    pw_node_t node;
    struct pw_loop *loop;
    struct pw_stream *stream;
    struct spa_video_info format;
    capture_settings_t cap_set;
} pw_data_t;

typedef struct {
    struct pw_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_registry *registry;
    struct spa_hook registry_listener;
} pw_mon_t;

static void build_format(struct spa_pod_builder *b, const struct spa_pod **params);
static uint32_t control_to_prop_id(uint32_t control_id);

SENSOR(PW_NAME);

static pw_mon_t pw_mon;
static pw_data_t *last_recved, *first_node;
static map_t *nodes;
static int uid;

static void free_node(void *dev) {
    pw_data_t *pw = (pw_data_t *)dev;
    pw->node.action = NULL; // see destroy_dev() impl
    destroy_dev(pw);
    pw_proxy_destroy(pw->node.proxy);
    free(pw);
}

static void _ctor_ init_libpipewire(void) {
    pw_init(NULL, NULL);
    nodes = map_new(true, free_node);
}

static void _dtor_ destroy_libpipewire(void) {
    map_free(nodes);
    pw_deinit();
}

static void set_env() {
    if (bus_sender_runtime_dir()) {
        setenv("XDG_RUNTIME_DIR", bus_sender_runtime_dir(), 1);
    } else {
        char path[64];
        snprintf(path, sizeof(path), "/run/user/%d", uid);
        setenv("XDG_RUNTIME_DIR", path, 1);;
    }
}

static void on_process(void *_data) {
    pw_data_t *pw = _data;
    
    struct pw_buffer *b = pw_stream_dequeue_buffer(pw->stream);
    if (b == NULL) {
        fprintf(stderr, "out of buffers: %m");
        goto err;
    }
    
    const bool is_yuv = pw->format.info.raw.format == SPA_VIDEO_FORMAT_YUY2;
    struct spa_buffer *buf = b->buffer;
    uint8_t *sdata = buf->datas[0].data;
    if (sdata == NULL) {
        goto err;
    }
    
    rect_info_t full = {
        .row_start = 0,
        .row_end = pw->cap_set.height,
        .col_start = 0,
        .col_end = pw->cap_set.width,
    };
    pw->cap_set.pct[pw->cap_set.capture_idx++] = get_frame_brightness(sdata, &full, is_yuv);
    pw_stream_queue_buffer(pw->stream, b);
    return;
    
err:
    pw->cap_set.with_err = true;
}

static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param) {
    pw_data_t *pw = _data;
    
    if (param == NULL || id != SPA_PARAM_Format) {
        return;
    }
    
    if (spa_format_parse(param,
        &pw->format.media_type,
        &pw->format.media_subtype) < 0) {
        
        pw->cap_set.with_err = true;
        return;
    }
        
    if (pw->format.media_type != SPA_MEDIA_TYPE_video ||
        pw->format.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
        
        pw->cap_set.with_err = true;
        return;
    }
            
    if (spa_format_video_raw_parse(param, &pw->format.info.raw) < 0) {
        pw->cap_set.with_err = true;
        return;
    }
    
    uint8_t params_buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
    const struct spa_pod *params = spa_pod_builder_add_object(&b,
                                         SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                                         SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1<<SPA_DATA_MemPtr)));
    pw_stream_update_params(pw->stream, &params, 1);
    
    pw->cap_set.width = pw->format.info.raw.size.width;
    pw->cap_set.height =  pw->format.info.raw.size.height;
    
    INFO("Image fmt: %d\n", pw->format.info.raw.format);
    INFO("Image res: %d x %d\n", pw->cap_set.width, pw->cap_set.height);
}

/* these are the stream events we listen for */
static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void build_format(struct spa_pod_builder *b, const struct spa_pod **params) {
    *params = spa_pod_builder_add_object(b,
                                        SPA_TYPE_OBJECT_Format,     SPA_PARAM_EnumFormat,
                                        SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
                                        SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                                        SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(2,
                                                                        SPA_VIDEO_FORMAT_GRAY8, // V4L2_PIX_FMT_GREY
                                                                        SPA_VIDEO_FORMAT_YUY2), // V4L2_PIX_FMT_YUYV
                                        SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
                                                                        &SPA_RECTANGLE(160, 120),
                                                                        &SPA_RECTANGLE(1, 1),
                                                                        &SPA_RECTANGLE(640, 480)),
                                        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                                                                        &SPA_FRACTION(25, 1),
                                                                        &SPA_FRACTION(0, 1),
                                                                        &SPA_FRACTION(120, 1)));
}

static bool validate_dev(void *dev) {
    return true;
}

static void fetch_dev(const char *interface, void **dev) {
    pw_data_t *pw = NULL;
    if (interface && strlen(interface)) {
        uint32_t id = atoi(interface);
        for (map_itr_t *itr = map_itr_new(nodes); itr; itr = map_itr_next(itr)) {
            pw_data_t *node = map_itr_get_data(itr);
            if (node->node.id == id) {
                pw = node;
                free(itr);
                break;
            }
        }
    } else {
        pw = first_node;
    }
    *dev = pw;
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    static char str_id[32];
    pw_data_t *pw = (pw_data_t *)dev;
    sprintf(str_id, "%d", pw->node.id);
    *node = str_id;
    if (action) {
        *action = pw->node.action;
    }
}

static void destroy_dev(void *dev) {
    pw_data_t *pw = (pw_data_t *)dev;
    if (pw->stream) {
        pw_stream_destroy(pw->stream);
        pw->stream = NULL;
    }
    if (pw->loop) {
        pw_loop_destroy(pw->loop);
        pw->loop = NULL;
    }
    
    memset(&pw->cap_set, 0, sizeof(pw->cap_set));
    
    if (pw->node.action && !strcmp(pw->node.action, UDEV_ACTION_RM)) {
        BUILD_KEY(pw->node.id);
        map_remove(nodes, key);
    }
}

static void removed_proxy(void *data) {
    pw_data_t *pw = (pw_data_t *)data;
    pw->node.action = UDEV_ACTION_RM; // this will kill our module during destroy_dev() call!
    INFO("Removed node %d\n", pw->node.id);
    last_recved = pw;
}

static const struct pw_proxy_events proxy_events = {
    PW_VERSION_PROXY_EVENTS,
    .removed = removed_proxy,
};

static void registry_event_global(void *data, uint32_t id,
                                uint32_t permissions, const char *type, uint32_t version,
                                const struct spa_dict *props) {

    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char *mc = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        const char *mr = spa_dict_lookup(props, PW_KEY_MEDIA_ROLE);
        if (mc && mr &&  !strcmp(mc, "Video/Source") && !strcmp(mr, "Camera")) {
            pw_data_t *pw = calloc(1, sizeof(pw_data_t));
            if (!pw) {
                return;
            }
            INFO("Added node %d\n", id);
            pw->node.id = id;
            pw->node.action = UDEV_ACTION_ADD;
            pw->node.proxy = pw_registry_bind(pw_mon.registry, id, type, PW_VERSION_NODE, sizeof(pw_data_t));
            pw_proxy_add_listener(pw->node.proxy, &pw->node.proxy_listener, &proxy_events, pw);
            last_recved = pw; // used by recv_monitor
            if (map_length(nodes) == 0) {
                first_node = pw;
            }
            BUILD_KEY(id);
            map_put(nodes, key, pw);
        }
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
};

static int init_monitor(void) {
    /* 
     * Pipewire needs an XDG_RUNTIME_DIR set;
     * at this phase, we are not being called by anyone;
     * thus we need to workaround this by fetching
     * a real user id, and use it to build the XDG_RUNTIME_DIR env.
     */
    setpwent();
    char path[64];
    for (struct passwd *p = getpwent(); p; p = getpwent()) {
        snprintf(path, sizeof(path), "/run/user/%d/", p->pw_uid);
        if (access(path, F_OK) == 0) {
            uid = p->pw_uid;
            break;
        }
    }
    endpwent();
    
    // Unsupported... can this happen?
    if (uid == 0) {
        return -1;
    }
    
    set_env();
    
    pw_mon.loop = pw_loop_new(NULL);
    if (!pw_mon.loop) {
        return -1;
    }
    pw_mon.context = pw_context_new(pw_mon.loop, NULL, 0);
    if (!pw_mon.context) {
        return -1;
    }
    pw_mon.core = pw_context_connect(pw_mon.context, NULL, 0);
    if (!pw_mon.core) {
        return -1;
    }
    pw_mon.registry = pw_core_get_registry(pw_mon.core, PW_VERSION_REGISTRY, 0);
    if (!pw_mon.registry) {
        return -1;
    }
    
    spa_zero(pw_mon.registry_listener);
    pw_registry_add_listener(pw_mon.registry, &pw_mon.registry_listener, &registry_events, NULL);
    
    int fd = pw_loop_get_fd(pw_mon.loop);
    pw_loop_enter(pw_mon.loop);

    unsetenv("XDG_RUNTIME_DIR");
    return fd;
}

static void recv_monitor(void **dev) {
    last_recved = NULL;
    pw_loop_iterate(pw_mon.loop, 0);
    *dev = last_recved;
}

static void destroy_monitor(void) {
    if (pw_mon.registry) {
        pw_proxy_destroy((struct pw_proxy*)pw_mon.registry);
    }
    if (pw_mon.core) {
        pw_core_disconnect(pw_mon.core);
    }
    if (pw_mon.context) {
        pw_context_destroy(pw_mon.context);
    }
    if (pw_mon.loop) {
        pw_loop_destroy(pw_mon.loop);
    }
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    pw_data_t *pw = (pw_data_t *)dev;
    pw->cap_set.pct = pct;

    set_env();
    
    const struct spa_pod *params;
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
    pw->loop = pw_loop_new(NULL);
    pw->stream = pw_stream_new_simple(pw->loop,
                                      "clightd-camera-pw",
                                      pw_properties_new(
                                          PW_KEY_MEDIA_TYPE, "Video",
                                          PW_KEY_MEDIA_CATEGORY, "Capture",
                                          PW_KEY_MEDIA_ROLE, "Camera",
                                          NULL),
                                      &stream_events,
                                      pw);
    build_format(&b, &params);
    int res;
    if ((res = pw_stream_connect(pw->stream,
        PW_DIRECTION_INPUT,
        pw->node.id,
        PW_STREAM_FLAG_AUTOCONNECT |    /* try to automatically connect this stream */
        PW_STREAM_FLAG_MAP_BUFFERS,     /* mmap the buffer data for us */
        &params, 1))                  /* extra parameters, see above */ < 0) {
        
        fprintf(stderr, "Can't connect: %s\n", spa_strerror(res));
    } else {
        pw_loop_enter(pw->loop);
        while (pw->cap_set.capture_idx < num_captures && !pw->cap_set.with_err) {
            if (pw_loop_iterate(pw->loop, -1) < 0) {
                break;
            }
            if (pw->cap_set.width != 0) {
                // Settings must be set once we receive on_params_changed()
                set_camera_settings(pw, settings);
            }
        }
        pw_loop_leave(pw->loop);
        restore_camera_settings(pw);
    }
    unsetenv("XDG_RUNTIME_DIR");
    return pw->cap_set.capture_idx;
}

// Stolen from https://github.com/PipeWire/pipewire/blob/master/spa/plugins/v4l2/v4l2-utils.c#L1049
static inline enum spa_prop control_to_prop_id(uint32_t control_id) {
    switch (control_id) {
        case V4L2_CID_BRIGHTNESS:
            return SPA_PROP_brightness;
        case V4L2_CID_CONTRAST:
            return SPA_PROP_contrast;
        case V4L2_CID_SATURATION:
            return SPA_PROP_saturation;
        case V4L2_CID_HUE:
            return SPA_PROP_hue;
        case V4L2_CID_GAMMA:
            return SPA_PROP_gamma;
        case V4L2_CID_EXPOSURE:
            return SPA_PROP_exposure;
        case V4L2_CID_GAIN:
            return SPA_PROP_gain;
        case V4L2_CID_SHARPNESS:
            return SPA_PROP_sharpness;
        default:
            return SPA_PROP_START_CUSTOM + control_id;
    }
}

static struct v4l2_control *set_camera_setting(void *priv, uint32_t op, float val, const char *op_name, bool store) {
    pw_data_t *pw = (pw_data_t *)priv;
    enum spa_prop pw_op = control_to_prop_id(op);
    const struct pw_stream_control *ctrl = pw_stream_get_control(pw->stream, pw_op);
    if (ctrl) {
        INFO("%s (%u) default val: %.2lf\n", op_name, pw_op, ctrl->def);
        if (val < 0) {
            val = ctrl->def;
        }
        if (ctrl->values[0] != val) {
            float old_val = ctrl->values[0];
            int ret = pw_stream_set_control(pw->stream, pw_op, 1, &val);
            if (ret == 0) {
                INFO("Set '%s' val: %.2lf\n", op_name, val);
                if (store) {
                    INFO("Storing initial setting for '%s': %.2lf\n", op_name, old_val);
                    struct v4l2_control *v = malloc(sizeof(struct v4l2_control));
                    v->value = old_val;
                    v->id = op;
                    return v;
                }
            } else {
                INFO("Failed to set '%s'.\n", op_name);
            }
        } else {
            INFO("Value %2.lf for '%s' already set.\n", val, ctrl->name);
        }
    } else {
        INFO("'%s' unsupported\n", op_name);
    }
    return NULL;
}

static int try_set_crop(void *priv, crop_info_t *crop, crop_type_t *crop_type)  {
    pw_data_t *pw = (pw_data_t *)priv;
    uint8_t params_buffer[1024];
    
    struct spa_meta_region *reg = NULL;
    if (crop != NULL) {
        reg = malloc(sizeof(struct spa_meta_region));
        reg->region.size.width = (crop[X_AXIS].area_pct[1] - crop[X_AXIS].area_pct[0]) * pw->format.info.raw.size.width;
        reg->region.size.height = (crop[Y_AXIS].area_pct[1] - crop[Y_AXIS].area_pct[0]) * pw->format.info.raw.size.height;
        reg->region.position.x = crop[X_AXIS].area_pct[0] * pw->format.info.raw.size.width;
        reg->region.position.y = crop[Y_AXIS].area_pct[0] * pw->format.info.raw.size.height;
    }
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
    const struct spa_pod *params = spa_pod_builder_add_object(&b,
                                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta, 
                                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoCrop), 
                                                              SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_region)), reg);
    free(reg);
    int ret = pw_stream_update_params(pw->stream, &params, 1);
    if (ret >= 0) {
        *crop_type = CROP_API;
        pw->cap_set.width *= crop[X_AXIS].area_pct[1] - crop[X_AXIS].area_pct[0];
        pw->cap_set.height *= crop[Y_AXIS].area_pct[1] - crop[Y_AXIS].area_pct[0];
        ret = 0;
    }
    return ret; 
}

#endif
