#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sensor.h>
#include <udev.h>
#include <jpeglib.h>
#include <module/map.h>

#define CAMERA_NAME                 "Camera"
#define CAMERA_ILL_MAX              255
#define CAMERA_SUBSYSTEM            "video4linux"
#define HISTOGRAM_STEPS             40

#define SET_V4L2(id, val)           set_v4l2_control(id, val, #id, true)
#define SET_V4L2_DEF(id)            set_v4l2_control_def(id, #id)
#ifndef NDEBUG
    #define INFO(fmt, ...)          printf(fmt, ##__VA_ARGS__);
#else
    #define INFO(fmt, ...)
#endif

struct buffer {
    uint8_t *start;
    size_t length;
};

struct histogram {
    double count;
    double sum;
};

struct mjpeg_dec {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr err;
    int (*dec_cb)(uint8_t **frame, int len);
};

typedef enum { X_AXIS, Y_AXIS, MAX_AXIS } crop_axis;
typedef enum { DISABLED, CROP_API, SELECTION_API, MANUAL} crop_type_t;

typedef struct {
    bool enabled;
    double area_pct[2]; // start - end
} crop_info_t;

struct state {
    int device_fd;
    uint32_t pixelformat;
    uint32_t width;
    uint32_t height;
    crop_info_t crop[MAX_AXIS];
    crop_type_t crop_type;
    struct buffer buf;
    struct histogram hist[HISTOGRAM_STEPS];
    char *settings;
    struct mjpeg_dec *decoder;
    map_t *stored_values;
};

static void set_v4l2_control_def(uint32_t id, const char *name);
static void set_v4l2_control(uint32_t id, int32_t val, const char *name, bool store);
static void set_camera_settings_def(void);
static void inline fill_crop_rect(crop_info_t *cr, struct v4l2_rect *rect);
static int set_selection(crop_info_t *cr);
static int set_crop(crop_info_t *cr);
static int try_set_crop(crop_info_t *crop);
static void set_camera_settings(void);
static void restore_camera_settings(void);
static int set_camera_fmt(void);
static int check_camera_caps(void);
static void create_decoder(void);
static int mjpeg_to_gray(uint8_t **img_data, int size);
static void destroy_decoder(void);
static int init_mmap(void);
static void destroy_mmap(void);
static int xioctl(int request, void *arg);
static int start_stream(void);
static int stop_stream(void);
static int send_frame(struct v4l2_buffer *buf);
static int recv_frame(struct v4l2_buffer *buf);
static double compute_brightness(unsigned int size);

static struct state state;
static struct udev_monitor *mon;
static const __u32 supported_fmts[] = {
    V4L2_PIX_FMT_GREY,
    V4L2_PIX_FMT_YUYV,
    V4L2_PIX_FMT_MJPEG
};

SENSOR(CAMERA_NAME);

static bool validate_dev(void *dev) {
    state.device_fd = open(udev_device_get_devnode(dev), O_RDWR);
    if (state.device_fd >= 0) {
        return check_camera_caps() == 0;
    }
    /* Always return true if action is "remove", ie: when called by udev monitor */
    const char *action = udev_device_get_action(dev);
    return action && !strcmp(action, UDEV_ACTION_RM);
}

static void fetch_dev(const char *interface, void **dev) {
    get_udev_device(interface, CAMERA_SUBSYSTEM, NULL, NULL, (struct udev_device **)dev);
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    if (node) {
        *node =  udev_device_get_devnode(dev);
    }
    if (action) {
        *action = udev_device_get_action(dev);
    }
}

static void destroy_dev(void *dev) {
    udev_device_unref(dev);
    if (state.device_fd >= 0) {
        close(state.device_fd);
    }
    map_free(state.stored_values);
    /* reset state */
    memset(&state, 0, sizeof(struct state));
    state.device_fd = -1;
}

static int init_monitor(void) {
    return init_udev_monitor(CAMERA_SUBSYSTEM, &mon);
}

static void recv_monitor(void **dev) {
    *dev = udev_monitor_receive_device(mon);
}

static void destroy_monitor(void) {
    udev_monitor_unref(mon);
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    state.settings = settings;
    state.stored_values = map_new(true, free);
    int ctr = 0;
    
    if (set_camera_fmt() == 0 && init_mmap() == 0 && start_stream() == 0) {
        set_camera_settings();
        create_decoder();
        for (int i = 0; i < num_captures; i++) {
            struct v4l2_buffer buf = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
            memset(state.hist, 0, HISTOGRAM_STEPS * sizeof(struct histogram));
            
            if (send_frame(&buf) == 0 && recv_frame(&buf) == 0) {
                pct[ctr++] = compute_brightness(buf.bytesused) / CAMERA_ILL_MAX;
            }
        }
        destroy_decoder();
        stop_stream();
        restore_camera_settings();
    }
    destroy_mmap();
    return ctr;
}

static void set_v4l2_control_def(uint32_t id, const char *name) {
    struct v4l2_queryctrl arg = {0};
    arg.id = id;
    if (-1 == xioctl(VIDIOC_QUERYCTRL, &arg)) {
        INFO("%s unsupported\n", name);
    } else {
        INFO("%s (%u) default val: %d\n", name, id, arg.default_value);
        set_v4l2_control(id, arg.default_value, name, true);
    }
}

static void set_v4l2_control(uint32_t id, int32_t val, const char *name, bool store) {
    struct v4l2_control old_ctrl = {0};
    old_ctrl.id = id;
    /* Store initial value, if set. */
    if (-1 == xioctl(VIDIOC_G_CTRL, &old_ctrl)) {
        INFO("'%s' unsupported\n", name);
        return;
    }
    
    if (old_ctrl.value != val) {
        struct v4l2_control ctrl ={0};
        ctrl.id = id;
        ctrl.value = val;
        if (-1 == xioctl(VIDIOC_S_CTRL, &ctrl)) {
            INFO("%s unsupported\n", name);
        } else {
            INFO("Set '%s' val: %d\n", name, val);
            if (store) {
                struct v4l2_control *store_ctrl = calloc(1, sizeof(struct v4l2_control));
                if (store_ctrl) {
                    memcpy(store_ctrl, &old_ctrl, sizeof(struct v4l2_control));
                    INFO("Storing initial setting for '%s': %d\n", name, val);
                    map_put(state.stored_values, name, (void *)store_ctrl);
                } else {
                    INFO("failed to store initial setting for '%s'\n", name)
                }
            }
        }
    } else {
        INFO("Value %d for '%s' already set.\n", val, name);
    }
}

/* Properly set everything to default value */
static void set_camera_settings_def(void) {
    SET_V4L2_DEF(V4L2_CID_SCENE_MODE);
    SET_V4L2_DEF(V4L2_CID_AUTO_WHITE_BALANCE);
    SET_V4L2_DEF(V4L2_CID_EXPOSURE_AUTO);
    SET_V4L2_DEF(V4L2_CID_AUTOGAIN);
    SET_V4L2_DEF(V4L2_CID_ISO_SENSITIVITY_AUTO);
    SET_V4L2_DEF(V4L2_CID_BACKLIGHT_COMPENSATION);
    SET_V4L2_DEF(V4L2_CID_AUTOBRIGHTNESS);
    
    SET_V4L2_DEF(V4L2_CID_WHITE_BALANCE_TEMPERATURE);
    SET_V4L2_DEF(V4L2_CID_EXPOSURE_ABSOLUTE);
    SET_V4L2_DEF(V4L2_CID_IRIS_ABSOLUTE);
    SET_V4L2_DEF(V4L2_CID_GAIN);
    SET_V4L2_DEF(V4L2_CID_ISO_SENSITIVITY);
    SET_V4L2_DEF(V4L2_CID_BRIGHTNESS);
}

static void inline fill_crop_rect(crop_info_t *cr, struct v4l2_rect *rect) {
    if (state.crop[Y_AXIS].enabled) {
        const double *a_pct = cr[Y_AXIS].area_pct;
        const double height_pct = a_pct[1] - a_pct[0];
        rect->height = height_pct * state.height;
        rect->top = a_pct[0] * state.height;
    }
    if (state.crop[X_AXIS].enabled) {
        const double *a_pct = cr[X_AXIS].area_pct;
        const double width_pct = a_pct[1] - a_pct[0];
        rect->width = width_pct * state.width;
        rect->left = a_pct[0] * state.width;
    }
}

// https://www.kernel.org/doc/html/v4.12/media/uapi/v4l/vidioc-g-selection.html
static int set_selection(crop_info_t *cr) {    
    struct v4l2_selection selection = {0};
    selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    selection.target = V4L2_SEL_TGT_CROP;
    if (-1 == xioctl(VIDIOC_G_SELECTION, &selection)) {
        INFO("VIDIOC_G_SELECTION failed: %m\n");
        return -errno;
    }    
    if (cr) {
        fill_crop_rect(cr, &selection.r);
    } else {
        // Reset default
        selection.target = V4L2_SEL_TGT_CROP_DEFAULT;
    }
    if (-1 == xioctl(VIDIOC_S_SELECTION, &selection)) {
        INFO("VIDIOC_S_SELECTION failed: %m\n");
        return -errno;
    }
    state.crop_type = SELECTION_API;
    return 0;
}

// https://www.kernel.org/doc/html/v4.14/media/uapi/v4l/crop.html
static int set_crop(crop_info_t *cr) {
    struct v4l2_crop crop = {0};
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(VIDIOC_G_CROP, &crop)) {
        INFO("VIDIOC_G_CROP failed: %m\n");
        return -errno;
    }
    
    if (cr) {
        fill_crop_rect(cr, &crop.c);
    } else {
        // Reset default
        struct v4l2_cropcap cropcap = {0};
        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(VIDIOC_CROPCAP, &cropcap)) {
            INFO("VIDIOC_CROPCAP failed: %m\n");
            return -errno;
        }
        crop.c = cropcap.defrect;
    }
    if (-1 == xioctl(VIDIOC_S_CROP, &crop)) {
        INFO("VIDIOC_S_CROP failed: %m\n");
        return -errno;
    }
    state.crop_type = CROP_API;
    return 0;
}

static int try_set_crop(crop_info_t *crop) {
    /** Try "new" selection API **/
    if (set_selection(crop) != 0) {
        /** Try "old" crop API **/
        return set_crop(crop);
    }
    return 0;
}

/* Parse settings string! */
static void set_camera_settings(void) {
    /* Set default values */
    set_camera_settings_def();
    if (state.settings && strlen(state.settings)) {
        char *token; 
        char *rest = state.settings;
                
        while ((token = strtok_r(rest, ",", &rest))) {
            uint32_t v4l2_op;
            int32_t v4l2_val;
            char axis;
            double area_pct[2];
            
            if (sscanf(token, "%u=%d", &v4l2_op, &v4l2_val) == 2) {
                SET_V4L2(v4l2_op, v4l2_val);
            } else if (sscanf(token, "%c=%lf-%lf", &axis, &area_pct[0], &area_pct[1]) == 3) {
                int8_t crop_idx = -1;
                if (area_pct[0] >= area_pct[1]) {
                    fprintf(stderr, "Start should be lesser than end: %lf-%lf\n", area_pct[0], area_pct[1]);
                } else {
                    switch (axis) {
                        case 'x':
                            crop_idx = X_AXIS;
                            break;
                        case 'y':
                            crop_idx = Y_AXIS;
                            break;
                        default:
                            fprintf(stderr, "wrong axis specified: %c; 'x' or 'y' supported.\n", axis);
                            break;
                    }
                }
                if (crop_idx != -1 && !state.crop[crop_idx].enabled) {
                    state.crop[crop_idx].enabled = true;
                    state.crop[crop_idx].area_pct[0] = area_pct[0];
                    state.crop[crop_idx].area_pct[1] = area_pct[1];
                }
            } else {
                fprintf(stderr, "Expected a=b format in '%s' token.\n", token);
            }
        }
        if (state.crop[X_AXIS].enabled || state.crop[Y_AXIS].enabled) {
            if (try_set_crop(state.crop) != 0) {
                INFO("Unsupported crop/selection v4l2 API; fallback at manually skipping pixels.\n")
                state.crop_type = MANUAL;
            }
        }
    }
}

static void restore_camera_settings(void) {
    for (map_itr_t *itr = map_itr_new(state.stored_values); itr; itr = map_itr_next(itr)) {
        struct v4l2_control *old_ctrl = map_itr_get_data(itr);
        const char *ctrl_name = map_itr_get_key(itr); 
        INFO("Restoring setting for '%s'\n", ctrl_name)
        set_v4l2_control(old_ctrl->id, old_ctrl->value, ctrl_name, false);
    }
    
    // Restore crop if needed
    switch (state.crop_type) {
    case SELECTION_API:
        set_selection(NULL);
        break;
    case CROP_API:
        set_crop(NULL);
        break;
    default:
        break;
    }
}

static int set_camera_fmt(void) {
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 160;
    fmt.fmt.pix.height = 120;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;   
    fmt.fmt.pix.pixelformat = state.pixelformat;
    if (-1 == xioctl(VIDIOC_S_FMT, &fmt)) {
        perror("Setting Pixel Format");
        return -1;
    }
    
    INFO("Image fmt: %s\n", (char *)&fmt.fmt.pix.pixelformat);
    INFO("Image res: %d x %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    state.height = fmt.fmt.pix.height;
    state.width = fmt.fmt.pix.width;
    return 0;
}

static int check_camera_caps(void) {
    struct v4l2_capability caps = {{0}};
    if (-1 == xioctl(VIDIOC_QUERYCAP, &caps)) {
        perror("Querying Capabilities");
        return -1;
    }
    
    /* Check if it is a capture dev */
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        perror("No video capture device");
        return -1;
    }
    
    /* Check if it does support streaming */
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        perror("Device does not support streaming i/o");
        return -1;
    }
    
    /* Try to set lowest device priority level. No need to quit if this is not supported. */
    enum v4l2_priority priority = V4L2_PRIORITY_BACKGROUND;
    if (-1 == xioctl(VIDIOC_S_PRIORITY, &priority)) {
        INFO("Failed to set priority\n");
    }
    
    /* Check supported formats */
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (xioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0 && state.pixelformat == 0) {
        for (int i = 0; i < SIZE(supported_fmts); i++) {
            if (fmtdesc.pixelformat == supported_fmts[i]) {
                state.pixelformat = supported_fmts[i];
            }
        }
        fmtdesc.index++;
    }
    
    /* No supported formats found? */
    if (state.pixelformat == 0) {
        perror("Device does not support neither GREY nor YUYV nor MJPEG pixelformats.");
        return -1;
    }
    return 0;
}

static void create_decoder(void) {
    if (state.pixelformat == V4L2_PIX_FMT_MJPEG) {
        state.decoder = malloc(sizeof(*state.decoder));
        state.decoder->cinfo.err = jpeg_std_error(&state.decoder->err);
        jpeg_create_decompress(&state.decoder->cinfo);
        state.decoder->dec_cb = mjpeg_to_gray;
    }
}

static int mjpeg_to_gray(uint8_t **img_data, int size) {
     /* Decompress jpeg and convert to grayscale through libjpeg */
    jpeg_mem_src(&state.decoder->cinfo, *img_data, size);
    int rc = jpeg_read_header(&state.decoder->cinfo, TRUE);
    if (rc != 1) {
        INFO("File does not seem to be a normal JPEG");
        return 0;
    }
        
    /* Convert from RGB to grayscale */
    state.decoder->cinfo.out_color_space = JCS_GRAYSCALE;
        
    jpeg_start_decompress(&state.decoder->cinfo);
    const int width = state.decoder->cinfo.output_width;
    const int height = state.decoder->cinfo.output_height;
    const int pixel_size = state.decoder->cinfo.output_components;
    const int row_stride = width * pixel_size;
    const int bmp_size = row_stride * height;
    
    *img_data = malloc(bmp_size);
    if (*img_data) {
        while (state.decoder->cinfo.output_scanline < height) {
            unsigned char *buffer_array = *img_data + state.decoder->cinfo.output_scanline * row_stride;
            jpeg_read_scanlines(&state.decoder->cinfo, &buffer_array, 1);
        }
        jpeg_finish_decompress(&state.decoder->cinfo);
        return bmp_size;
    }
    return -ENOMEM;
}

static void destroy_decoder(void) {
    if (state.decoder) {
        if (state.pixelformat == V4L2_PIX_FMT_MJPEG) {
            jpeg_destroy_decompress(&state.decoder->cinfo);
        }
        free(state.decoder);
    }
}

static int init_mmap(void) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req)) {
        perror("Requesting Buffer");
        return -1;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (-1 == xioctl(VIDIOC_QUERYBUF, &buf)) {
        perror("Querying Buffer");
        return -1;
    }
        
    state.buf.start = mmap(NULL,
                            buf.length,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            state.device_fd, buf.m.offset);
        
    if (MAP_FAILED == state.buf.start) {
        perror("mmap");
        return -1;
    }
    state.buf.length = buf.length;
    return 0;
}

static void destroy_mmap(void) {
    if (state.buf.length > 0) {
        munmap(state.buf.start, state.buf.length);
    }
}

static int xioctl(int request, void *arg) {
    int r;
    do {
        r = ioctl(state.device_fd, request, arg);
    } while (r == -1 && EINTR == errno);
    return r;
}

static int start_stream(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(VIDIOC_STREAMON, &type)) {
        perror("Start Capture");
        return -1;
    }
    return 0;
}

static int stop_stream(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(VIDIOC_STREAMOFF, &type)) {
        perror("Stop Capture");
        return -1;
    }
    return 0;
}

static int send_frame(struct v4l2_buffer *buf) {
    /* Enqueue buffer */
    if (-1 == xioctl(VIDIOC_QBUF, buf)) {
        perror("VIDIOC_QBUF");
        return -1;
    }
    return 0;
}

static int recv_frame(struct v4l2_buffer *buf) {    
    /* Dequeue the buffer */
    if(-1 == xioctl(VIDIOC_DQBUF, buf)) {
        perror("VIDIOC_DQBUF");
        return -1;
    }
    return 0;
}

static double compute_brightness(unsigned int size) {
    double brightness = 0.0;
    double min = CAMERA_ILL_MAX;
    double max = 0.0;
        
    uint8_t *img_data = state.buf.start;
    if (state.decoder) {
        size = state.decoder->dec_cb(&img_data, size);
        if (size < 0) {
            return brightness;
        }
    }
    
    /*
     * If greyscale -> increment by 1. 
     * If YUYV -> increment by 2: we only want Y! 
     */
    const int inc = 1 + (state.pixelformat == V4L2_PIX_FMT_YUYV);

    // Manual crop if needed
    int col_start = 0;
    int col_end = state.width;
    int row_start = 0;
    int row_end = state.height;
    if (state.crop_type == MANUAL) {
        if (state.crop[X_AXIS].enabled) {
            col_start = state.crop[X_AXIS].area_pct[0] * state.width;
            col_end = state.crop[X_AXIS].area_pct[1] * state.width;
        }
        if (state.crop[Y_AXIS].enabled) {
            row_start = state.crop[Y_AXIS].area_pct[0] * state.height;
            row_end = state.crop[Y_AXIS].area_pct[1] * state.height;
        }
        INFO("Manual crop: rows[%d-%d], cols[%d-%d]\n", row_start, row_end, col_start, col_end);
    }
    
    /* Find minimum and maximum brightness */
    int total = 0; // compute total used pixels
    for (int row = row_start; row < row_end; row++) {
        for (int col = col_start; col < col_end * inc; col += inc) {
            const int idx = (row + 1) * col;
            if (img_data[idx] < min) {
                min = img_data[idx];
            }
            if (img_data[idx] > max) {
                max = img_data[idx];
            }
            total++;
        }
    }
    INFO("Total computed pixels: %d\n", total);

    /* Ok, we should never get in here */
    if (max == 0.0) {
        goto end;
    }

    /* Calculate histogram */
    const double step_size = (max - min) / HISTOGRAM_STEPS;
    for (int row = row_start; row < row_end; row++) {
        for (int col = col_start; col < col_end * inc; col += inc) {
            const int idx = (row + 1) * col;
            int bucket = (img_data[idx] - min) / step_size;
            if (bucket >= 0 && bucket < HISTOGRAM_STEPS) {
                state.hist[bucket].sum += img_data[idx];
                state.hist[bucket].count++;
            }
        }
    }

    /* Find (approximate) quartiles for histogram steps */
    const double quartile_size = (double)total / 4;
    double quartiles[3] = {0};
    int j = 0;
    for (int i = 0; i < HISTOGRAM_STEPS && j < 3; i++) {
        quartiles[j] += state.hist[i].count;
        if (quartiles[j] >= quartile_size) {
            quartiles[j] = (quartile_size / quartiles[j]) + i;
            j++;
        }
    }

    /*
     * Results may be clustered in a single estimated quartile, 
     * in which case consider full range.
     */
    int min_bucket = 0;
    int max_bucket = HISTOGRAM_STEPS - 1;
    if (quartiles[2] > quartiles[0]) {
        /* Trim outlier buckets via interquartile range */
        const double iqr = (quartiles[2] - quartiles[0]) * 1.5;
        min_bucket = quartiles[0] - iqr;
        max_bucket = quartiles[2] + iqr;
        if (min_bucket < 0) {
            min_bucket = 0;
        }
        if (max_bucket > HISTOGRAM_STEPS - 1) {
            max_bucket = HISTOGRAM_STEPS - 1;
        }
    }
    
    /*
     * Find the highest brightness bucket with
     * a significant sample count
     * and return the average brightness for it.
     */
    for (int i = max_bucket; i >= min_bucket; i--) {
        if (state.hist[i].count > step_size) {
            brightness = state.hist[i].sum / state.hist[i].count;
            break;
        }
    }
    
end:
    if (img_data != state.buf.start) {
        free(img_data);
    }
    return brightness;
}
