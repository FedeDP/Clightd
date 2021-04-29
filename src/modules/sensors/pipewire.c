#ifdef PIPEWIRE_PRESENT

#include "camera.h"

#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/utils/result.h>

#include <pipewire/pipewire.h>

#define PW_NAME                 "Camera_PW"

typedef struct {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_video_info format;
    double *pct;
    int num_captures;
    int capture_idx;
    char *settings;
    map_t *stored_values;
} pw_data_t;

typedef struct {
    uint32_t id;
    struct spa_hook proxy_listener;
    struct pw_proxy *proxy;
} pw_node_t;

typedef struct {
    struct pw_loop *mon_loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_registry *registry;
} pw_mon_t;

extern const struct pw_stream_control *pw_stream_get_control(struct pw_stream *stream, uint32_t id);
static void build_format(struct spa_pod_builder *b, const struct spa_pod **params);
static uint32_t control_to_prop_id(uint32_t control_id);
static void set_camera_setting(pw_data_t *pw, uint32_t op, float val, bool store);
static void set_camera_settings_def(pw_data_t *pw);
static void set_camera_settings(pw_data_t *pw);
static void restore_camera_settings(pw_data_t *pw);

SENSOR(PW_NAME);

static pw_mon_t pw_mon;

static void _ctor_ init_libpipewire(void) {
    pw_init(NULL, NULL);
}

static void _dtor_ destroy_libpipewire(void) {
    pw_deinit();
}

static void on_process(void *_data) {
    pw_data_t *data = _data;
    
    struct pw_buffer *b;
    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        fprintf(stderr, "out of buffers: %m");
        goto end;
    }

    const bool is_yuv = data->format.info.raw.format == SPA_VIDEO_FORMAT_YUY2;
    struct spa_buffer *buf = b->buffer;
    for (int i = 0; i < buf->n_datas; i++) {
        uint8_t *sdata;
        if ((sdata = buf->datas[i].data) == NULL) {
           goto end;
        }
        data->pct[data->capture_idx++] = get_frame_brightness(sdata, buf->datas[i].chunk->size, is_yuv);
        
        // Ok, we finished capturing frames
        if (data->capture_idx == data->num_captures) {
            goto end;
        }
    }
    pw_stream_queue_buffer(data->stream, b);
    return;
    
end:
    pw_main_loop_quit(data->loop);
}

static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param) {
    pw_data_t *data = _data;

    if (param == NULL || id != SPA_PARAM_Format) {
        return;
    }

    if (spa_format_parse(param,
            &data->format.media_type,
            &data->format.media_subtype) < 0) {
        pw_main_loop_quit(data->loop);
        return;
    }

    if (data->format.media_type != SPA_MEDIA_TYPE_video ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
        pw_main_loop_quit(data->loop);
        return;
    }

    if (spa_format_video_raw_parse(param, &data->format.info.raw) < 0) {
        pw_main_loop_quit(data->loop);
        return;
    }
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
    pw_data_t *pw = (pw_data_t *)dev;
    return pw_stream_set_active(pw->stream, true) == 0;
}

static void fetch_dev(const char *interface, void **dev) {
    // TODO
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    pw_data_t *pw = calloc(1, sizeof(pw_data_t));
    if (pw) {
        const struct spa_pod *params;
        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
        pw->loop = pw_main_loop_new(NULL);
        pw->stream = pw_stream_new_simple(
            pw_main_loop_get_loop(pw->loop),
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
        // FIXME: fix when correct device is passed
        if ((res = pw_stream_connect(pw->stream,
                PW_DIRECTION_INPUT,
                interface ? (uint32_t)atoi(interface) : PW_ID_ANY,
                PW_STREAM_FLAG_AUTOCONNECT |    /* try to automatically connect this stream */
                PW_STREAM_FLAG_INACTIVE |
                PW_STREAM_FLAG_MAP_BUFFERS,     /* mmap the buffer data for us */
                &params, 1))                    /* extra parameters, see above */ < 0) {
            
            fprintf(stderr, "can't connect: %s\n", spa_strerror(res));
            free(pw);
            return;
        }
        *dev = pw;
    }
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    static char str_id[32];
    pw_data_t *pw = (pw_data_t *)dev;
    uint32_t id = pw_stream_get_node_id(pw->stream);
    sprintf(str_id, "%d", id);
    *node = str_id;
    
    if (action) {
        // TODO
        *action = "";
    }
}

static void destroy_dev(void *dev) {
    pw_data_t *pw = (pw_data_t *)dev;
    pw_stream_destroy(pw->stream);
    pw_main_loop_destroy(pw->loop);
    map_free(pw->stored_values);
    free(dev);
}
/*
static void removed_proxy (void *data) {
    pw_node_t *node = (pw_node_t *)data;
    INFO("Removed node %d\n", node->id);
    // TODO: manage node removed
    pw_proxy_destroy(node->proxy);
    free(node);
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
            INFO("Added node %d\n", id);
            pw_node_t *node = calloc(1, sizeof(pw_node_t));
            node->id = id;
            node->proxy = pw_registry_bind(pw_mon.registry, id, type, PW_VERSION_NODE, sizeof(pw_node_t));
            pw_proxy_add_listener(node->proxy, &node->proxy_listener, &proxy_events, node);
        }
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
};*/

static int init_monitor(void) {
    // TODO ?
//     pw_mon.mon_loop = pw_loop_new(NULL);
//     pw_mon.context = pw_context_new(pw_mon.mon_loop, NULL, 0);
//     pw_mon.core = pw_context_connect(pw_mon.context, NULL, 0);                                
//     pw_mon.registry = pw_core_get_registry(pw_mon.core, PW_VERSION_REGISTRY, 0);                                
//                                                                                 
//     struct spa_hook registry_listener;       
//     spa_zero(registry_listener);                                            
//     pw_registry_add_listener(pw_mon.registry, &registry_listener, &registry_events, NULL);    
//         
//     int fd = pw_loop_get_fd(pw_mon.mon_loop);
//     pw_loop_enter(pw_mon.mon_loop);
//     return fd;
}

static void recv_monitor(void **dev) {
//     pw_loop_iterate(pw_mon.mon_loop, 0); 
//     *dev = NULL; // TODO
}

static void destroy_monitor(void) {
//     pw_loop_leave(pw_mon.mon_loop);
//     pw_proxy_destroy((struct pw_proxy*)pw_mon.registry);
//     pw_core_disconnect(pw_mon.core);
//     pw_context_destroy(pw_mon.context);
//     pw_loop_destroy(pw_mon.mon_loop);
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    pw_data_t *pw = (pw_data_t *)dev;
    pw->num_captures = num_captures;
    pw->pct = pct;
    pw->settings = settings;
    pw->stored_values = map_new(true, free);
    
    set_camera_settings(pw);
    pw_main_loop_run(pw->loop);
    restore_camera_settings(pw);
    return pw->capture_idx;
}

// Stolen from https://github.com/PipeWire/pipewire/blob/master/spa/plugins/v4l2/v4l2-utils.c#L1017
static uint32_t control_to_prop_id(uint32_t control_id) {
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

static void set_camera_setting(pw_data_t *pw, uint32_t op, float val, bool store) {
    enum spa_prop pw_op = control_to_prop_id(op);
    const struct pw_stream_control *ctrl = pw_stream_get_control(pw->stream, pw_op);
    if (ctrl) {
        INFO("%s (%u) default val: %.2lf\n", ctrl->name, op, ctrl->def);
        if (val < 0) {
            val = ctrl->def;
        }
        if (ctrl->values[0] != val) {
            float old_val = ctrl->values[0];
            pw_stream_set_control(pw->stream, pw_op, 1, &val);
            INFO("Set '%s' val: %.2lf\n", ctrl->name, val);
            if (store) {
                INFO("Storing initial setting for '%s': %.2lf\n", ctrl->name, old_val);
                struct v4l2_control *v = malloc(sizeof(struct v4l2_control));
                v->value = old_val;
                v->id = op;
                map_put(pw->stored_values, ctrl->name, v);
            }
        } else {
            INFO("Value %2.lf for '%s' already set.\n", val, ctrl->name);
        }
    } else {
        INFO("%u unsupported\n", op);
    }
}

static void set_camera_settings_def(pw_data_t *pw) {
    set_camera_setting(pw, V4L2_CID_SCENE_MODE, -1, true);
    set_camera_setting(pw, V4L2_CID_AUTO_WHITE_BALANCE, -1, true);
    set_camera_setting(pw, V4L2_CID_EXPOSURE_AUTO, -1, true);
    set_camera_setting(pw, V4L2_CID_AUTOGAIN, -1, true);
    set_camera_setting(pw, V4L2_CID_ISO_SENSITIVITY_AUTO, -1, true);
    set_camera_setting(pw, V4L2_CID_BACKLIGHT_COMPENSATION, -1, true);
    set_camera_setting(pw, V4L2_CID_AUTOBRIGHTNESS, -1, true);
    
    set_camera_setting(pw, V4L2_CID_WHITE_BALANCE_TEMPERATURE, -1, true);
    set_camera_setting(pw, V4L2_CID_EXPOSURE_ABSOLUTE, -1, true);
    set_camera_setting(pw, V4L2_CID_IRIS_ABSOLUTE, -1, true);
    set_camera_setting(pw, V4L2_CID_GAIN, -1, true);
    set_camera_setting(pw, V4L2_CID_ISO_SENSITIVITY, -1, true);
    set_camera_setting(pw, V4L2_CID_BRIGHTNESS, -1, true);
}

/* Parse settings string! */
static void set_camera_settings(pw_data_t *pw) {
    /* Set default values */
    set_camera_settings_def(pw);
    if (pw->settings && strlen(pw->settings)) {
        char *token; 
        char *rest = pw->settings; 
        
        while ((token = strtok_r(rest, ",", &rest))) {
            uint32_t v4l2_op;
            int32_t v4l2_val;
            if (sscanf(token, "%u=%d", &v4l2_op, &v4l2_val) == 2) {
                float val = v4l2_val;
                set_camera_setting(pw, v4l2_op, val, true);
            } else {
                fprintf(stderr, "Expected a=b format in '%s' token.\n", token);
            }
        }
    }
}

static void restore_camera_settings(pw_data_t *pw) {
    for (map_itr_t *itr = map_itr_new(pw->stored_values); itr; itr = map_itr_next(itr)) {
        struct v4l2_control *old_ctrl = map_itr_get_data(itr);
        const char *ctrl_name = map_itr_get_key(itr); 
        INFO("Restoring setting for '%s'\n", ctrl_name)
        set_camera_setting(pw, old_ctrl->id, old_ctrl->value, false);
    }
}

#endif
