#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include "camera.h"
#include <jpeglib.h>

#define CAMERA_NAME                 "Camera"
#define CAMERA_SUBSYSTEM            "video4linux"

struct buffer {
    uint8_t *start;
    size_t length;
};

struct mjpeg_dec {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr err;
    int (*dec_cb)(uint8_t **frame, int len);
};

struct state {
    int device_fd;
    uint32_t pixelformat;
    uint32_t width; // real width, can be cropped
    uint32_t height; // real height, can be cropped
    struct buffer buf;
    struct mjpeg_dec *decoder;
};

static void inline fill_crop_rect(crop_info_t *cr, struct v4l2_rect *rect);
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
    int ctr = 0;
    
    if (set_camera_fmt() == 0 && init_mmap() == 0 && start_stream() == 0) {
        set_camera_settings(&state, settings);
        create_decoder();
        for (int i = 0; i < num_captures; i++) {
            struct v4l2_buffer buf = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP};
            if (send_frame(&buf) == 0 && recv_frame(&buf) == 0) {
                pct[ctr++] = compute_brightness(buf.bytesused);
            }
        }
        destroy_decoder();
        stop_stream();
        restore_camera_settings(&state);
    }
    destroy_mmap();
    return ctr;
}

static struct v4l2_control *set_camera_setting(void *priv, uint32_t id, float val, const char *name, bool store) {
    struct v4l2_control old_ctrl = {0};
    old_ctrl.id = id;
    int32_t v = (int32_t)val;

    /* Store initial value, if set. */
    if (-1 == xioctl(VIDIOC_G_CTRL, &old_ctrl)) {
        INFO("'%s' unsupported\n", name);
        return NULL;
    }
    
    if (old_ctrl.value != v) {
        struct v4l2_control ctrl ={0};
        ctrl.id = id;
        ctrl.value = v;
        if (-1 == xioctl(VIDIOC_S_CTRL, &ctrl)) {
            INFO("%s unsupported\n", name);
        } else {
            INFO("Set '%s' val: %d\n", name, v);
            if (store) {
                struct v4l2_control *store_ctrl = calloc(1, sizeof(struct v4l2_control));
                if (store_ctrl) {
                    memcpy(store_ctrl, &old_ctrl, sizeof(struct v4l2_control));
                    INFO("Storing initial setting for '%s': %d\n", name, v);
                    return store_ctrl;
                } else {
                    INFO("failed to store initial setting for '%s'\n", name)
                }
            }
        }
    } else {
        INFO("Value %d for '%s' already set.\n", v, name);
    }
    return NULL;
}

static void inline fill_crop_rect(crop_info_t *cr, struct v4l2_rect *rect) {
    const double height_pct = cr[Y_AXIS].area_pct[1] - cr[Y_AXIS].area_pct[0];
    rect->height = height_pct * state.height;
    rect->top = cr[Y_AXIS].area_pct[0] * state.height;

    const double width_pct = cr[X_AXIS].area_pct[1] - cr[X_AXIS].area_pct[0];
    rect->width = width_pct * state.width;
    rect->left = cr[X_AXIS].area_pct[0] * state.width;
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
    return 0;
}

static int try_set_crop(void *priv, crop_info_t *crop, crop_type_t *crop_type) {
    int ret;
    int cr_type = *crop_type > 0 ? *crop_type : SELECTION_API;
    do {
        switch (cr_type) {
        case SELECTION_API:
            ret = set_selection(crop);
            break;
        case CROP_API:
            ret = set_crop(crop);
            break;
        default:
            break;
        }
    } while (ret != 0 && *crop_type != cr_type--);
    if (ret == 0) {
        *crop_type = cr_type;
        
        // Update our pixel size as we are cropping
        state.width *= crop[X_AXIS].area_pct[1] - crop[X_AXIS].area_pct[0];
        state.height *= crop[Y_AXIS].area_pct[1] - crop[Y_AXIS].area_pct[0];
    }
    return ret;
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
    uint8_t *img_data = state.buf.start;
    if (state.decoder) {
        size = state.decoder->dec_cb(&img_data, size);
        if (size < 0) {
            return brightness;
        }
    }
    
    rect_info_t full = {
        .row_start = 0,
        .row_end = state.height,
        .col_start = 0,
        .col_end = state.width,
    };
    brightness = get_frame_brightness(img_data, &full, (state.pixelformat == V4L2_PIX_FMT_YUYV));
    if (img_data != state.buf.start) {
        free(img_data);
    }
    return brightness;
}
