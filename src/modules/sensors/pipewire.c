#ifdef PIPEWIRE_PRESENT

#include "camera.h"
#include "bus_utils.h"
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/utils/result.h>
#include <pipewire/pipewire.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#define PW_NAME                 "Pipewire"
#define PW_ENV_NAME             "CLIGHTD_PIPEWIRE_RUNTIME_DIR"
#define BUILD_KEY(id) \
    char key[10]; \
    snprintf(key, sizeof(key), "%d", id);

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
    char *settings;
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

static void free_node(void *dev);
static void build_format(struct spa_pod_builder *b, const struct spa_pod **params);
static uint32_t control_to_prop_id(uint32_t control_id);
static int register_monitor_fd(const char *pw_runtime_dir);

SENSOR(PW_NAME);

MODULE(PW_NAME);

static pw_mon_t pw_mon;
static pw_data_t *last_recved;
static map_t *nodes;
static int efd = -1;
static bool pw_inited;

static void module_pre_start(void) {
    
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    nodes = map_new(true, free_node);
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        if (msg->fd_msg->userptr != NULL) {
            // Pipewire monitor event! Notify Sensor
            uint64_t u = 1;
            write(efd, &u, sizeof(uint64_t));
        } else {
            // Inotify event!
            char buffer[EVENT_BUF_LEN];
            size_t len = read(msg->fd_msg->fd, buffer, EVENT_BUF_LEN);
            int i = 0;
            while (i < len) {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                if (event->len && strcmp(event->name, "pipewire-0") == 0) {
                    if (register_monitor_fd(getenv(PW_ENV_NAME)) != 0) {
                        fprintf(stderr, "Failed to start pipewire monitor.\n");
                    } else {
                        // We're done with the inotify fd; close it.
                        m_deregister_fd(msg->fd_msg->fd);
                        break;
                    }
                }
                i += EVENT_SIZE + event->len;
            }
        }
    }
}

static void destroy(void) {
    map_free(nodes);
}

static void free_node(void *dev) {
    pw_data_t *pw = (pw_data_t *)dev;
    pw->node.action = NULL; // see destroy_dev() impl
    destroy_dev(pw);
    pw_proxy_destroy(pw->node.proxy);
    free(pw);
}

static void on_state_changed(void *_data, enum pw_stream_state old,
                                   enum pw_stream_state state, const char *error) {
    pw_data_t *pw = _data;
    if (state == PW_STREAM_STATE_STREAMING) {
        /* Camera entered streaming mode; set settings */
        set_camera_settings(pw, pw->cap_set.settings);
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
        .row_end = pw->format.info.raw.size.height,
        .col_start = 0,
        .col_end = pw->format.info.raw.size.width,
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
    
    INFO("Image fmt: %d\n", pw->format.info.raw.format);
    INFO("Image res: %d x %d\n", pw->format.info.raw.size.width, pw->format.info.raw.size.height);
}

/* these are the stream events we listen for */
static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_state_changed,
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
    if (interface && interface[0] != '\0') {
        pw = map_get(nodes, interface);
    } else {
        // Peek first pw node found
        map_itr_t *itr = map_itr_new(nodes);
        if (itr) {
            pw = map_itr_get_data(itr);
            free(itr);
        }
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
            BUILD_KEY(id);
            map_put(nodes, key, pw);
        }
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
};

static int register_monitor_fd(const char *pw_runtime_dir) {
    setenv("XDG_RUNTIME_DIR", pw_runtime_dir, 1);
    
    pw_init(NULL, NULL);
    pw_inited = true;
    int ret = -1;
    do {
        pw_mon.loop = pw_loop_new(NULL);
        if (!pw_mon.loop) {
            break;
        }
        pw_mon.context = pw_context_new(pw_mon.loop, NULL, 0);
        if (!pw_mon.context) {
            break;
        }
        pw_mon.core = pw_context_connect(pw_mon.context, NULL, 0);
        if (!pw_mon.core) {
            break;
        }
        pw_mon.registry = pw_core_get_registry(pw_mon.core, PW_VERSION_REGISTRY, 0);
        if (!pw_mon.registry) {
            break;
        }
        
        spa_zero(pw_mon.registry_listener);
        pw_registry_add_listener(pw_mon.registry, &pw_mon.registry_listener, &registry_events, NULL);
        
        int fd = pw_loop_get_fd(pw_mon.loop);
        m_register_fd(fd, false, &pw_mon);
        
        pw_loop_enter(pw_mon.loop);
        ret = 0;
    } while (false);
    
    unsetenv("XDG_RUNTIME_DIR");
    if (ret == -1) {
        destroy_monitor();
    }
    return ret;
}

static int init_monitor(void) {
    const char *pw_runtime_dir = getenv(PW_ENV_NAME);
    if (pw_runtime_dir && pw_runtime_dir[0] != '\0') {
        struct stat s;
        if (stat(pw_runtime_dir, &s) == -1 || !S_ISDIR(s.st_mode)) {
            fprintf(stderr, "Failed to stat '%s' or not a folder. Killing pipewire.\n", pw_runtime_dir);
            goto err;
        }
        
        /* 
        * Pipewire needs an XDG_RUNTIME_DIR set;
        * at this phase, we are not being called by anyone;
        * thus we need to workaround this by fetching the pipewire socket
        * for the XDG_RUNTIME_DIR specified in CLIGHTD_PIPEWIRE_RUNTIME_DIR env variable;
        * eventually being notified of socket creation using inotify mechanism.
        */
        char path[256];
        snprintf(path, sizeof(path), "%s/pipewire-0", pw_runtime_dir);
        if (access(path, F_OK) == 0) {
            // Pipewire socket already present! Register the monitor right away
            if (register_monitor_fd(pw_runtime_dir) == -1) {
                fprintf(stderr, "Failed to register monitor. Killing pipewire.\n");
                goto err;
            }
        } else {
            // Register an inotify watcher
            int inot_fd = inotify_init();
            if (inotify_add_watch(inot_fd, pw_runtime_dir, IN_CREATE) >= 0) {
                m_register_fd(inot_fd, true, NULL);
            } else {
                fprintf(stderr, "Failed to watch folder '%s': %m\n", pw_runtime_dir);
                close(inot_fd);
                goto err;
            }
        }
        
        // Return an eventfd to notify Sensor for new devices
        efd = eventfd(0, 0);
        return efd;
    }
    // Env not found. Disable.
    fprintf(stderr, "No '" PW_ENV_NAME "' env found. Killing pipewire.\n");
    
err:
    return -1;
}

static void recv_monitor(void **dev) {
    // Consume the eventfd
    uint64_t u;
    read(efd, &u, sizeof(uint64_t));
    
    // Actually search for new nodes
    last_recved = NULL;
    pw_loop_iterate(pw_mon.loop, 0);
    *dev = last_recved;
}

static void destroy_monitor(void) {
    if (!pw_inited) {
        return;
    }

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
    memset(&pw_mon, 0, sizeof(pw_mon));
    
    if (efd != -1) {
        close(efd);
    }
    
    pw_deinit();
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    pw_data_t *pw = (pw_data_t *)dev;
    pw->cap_set.pct = pct;
    pw->cap_set.settings = settings;
    
    setenv("XDG_RUNTIME_DIR", bus_sender_runtime_dir(), 1);
    
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
            /* Set default value */
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
        ret = 0;
    }
    return ret; 
}

#endif
