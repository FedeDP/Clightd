#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sensor.h>

#define CAMERA_NAME                 "Camera"
#define CAMERA_ILL_MAX              255
#define CAMERA_SUBSYSTEM            "video4linux"

#define SET_V4L2(id, val)           set_v4l2_control(id, val, #id)
#define SET_V4L2_DEF(id)            set_v4l2_contro_def(id, #id)
#ifdef NDEBUG
    #define INFO(fmt, ...)          printf(fmt, ##__VA_ARGS__);
#else
    #define INFO(fmt, ...)
#endif

static int recv_frames(const char *interface);
static void open_device(const char *interface);
static void set_v4l2_contro_def(uint32_t id, const char *name);
static void set_v4l2_control(uint32_t id, int32_t val, const char *name);
static void set_camera_settings(void);
static void init(void);
static void init_mmap(void);
static int xioctl(int request, void *arg, bool exit_on_error);
static void start_stream(void);
static void stop_stream(void);
static void send_frame(void);
static void recv_frame(int i);
static double compute_brightness(const unsigned int size);
static void free_all();

struct buffer {
    uint8_t *start;
    size_t length;
};

struct state {
    int quit;
    int width;
    int height;
    int device_fd;
    int num_captures;
    uint32_t pixelformat;
    double *brightness;
    struct buffer buf;
};

static struct state state;

SENSOR(CAMERA_NAME, CAMERA_SUBSYSTEM, NULL);

/*
 * Frame capturing method
 */
static int capture(struct udev_device *dev, double *pct, const int num_captures) {
    state.num_captures = num_captures;
    state.brightness = pct;
    int r = recv_frames(udev_device_get_devnode(dev));
    free_all();
    return -r;
}

static int recv_frames(const char *interface) {
    open_device(interface);
    if (!state.quit) {
        init();
        if (state.quit) {
            goto end;
        }
        init_mmap();
        if (state.quit) {
            goto end;
        }
        start_stream();
        if (state.quit) {
            goto end;
        }
        for (int i = 0; i < state.num_captures && !state.quit; i++) {
            if (i > 0) {
                send_frame();
            }
            recv_frame(i);
        }
        stop_stream();
    }
end:
    return state.quit;
}

static void open_device(const char *interface) {
    state.device_fd = open(interface, O_RDWR);
    if (state.device_fd == -1) {
        perror(interface);
        state.quit = errno;
    }
}

static void set_v4l2_contro_def(uint32_t id, const char *name) {
    struct v4l2_queryctrl arg = {0};
    arg.id = id;
    if (-1 == xioctl(VIDIOC_QUERYCTRL, &arg, false)) {
        INFO("%s unsupported\n", name);
    } else {
        set_v4l2_control(id, arg.default_value, name);
    }
}

static void set_v4l2_control(uint32_t id, int32_t val, const char *name) {
    struct v4l2_control ctrl ={0};
    ctrl.id = id;
    ctrl.value = val;
    if (-1 == xioctl(VIDIOC_S_CTRL, &ctrl, false)) {
        INFO("%s unsupported\n", name);
    }
}

/* Properly disable any automatic control and set everything to default value */
static void set_camera_settings(void) {
    // disable any scene mode
    SET_V4L2(V4L2_CID_SCENE_MODE, V4L2_SCENE_MODE_NONE);
    // disable auto white balance
    SET_V4L2(V4L2_CID_AUTO_WHITE_BALANCE, 0);
    // set default white balance
    SET_V4L2_DEF(V4L2_CID_WHITE_BALANCE_TEMPERATURE);
    // disable auto exposure
    SET_V4L2(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL);
    // set default exposure
    SET_V4L2_DEF(V4L2_CID_EXPOSURE_ABSOLUTE);
    // set default iris
    SET_V4L2_DEF(V4L2_CID_IRIS_ABSOLUTE);
    // disable auto iso
    SET_V4L2(V4L2_CID_ISO_SENSITIVITY_AUTO, V4L2_ISO_SENSITIVITY_MANUAL);
    // set default iso
    SET_V4L2_DEF(V4L2_CID_ISO_SENSITIVITY);
    // disable autogain
    SET_V4L2(V4L2_CID_AUTOGAIN, 0);
    // set default gain
    SET_V4L2_DEF(V4L2_CID_GAIN);
    // disable backlight compensation
    SET_V4L2(V4L2_CID_BACKLIGHT_COMPENSATION, 0);
    // disable autobrightness
    SET_V4L2(V4L2_CID_AUTOBRIGHTNESS, 0);
    // set default brightness
    SET_V4L2_DEF(V4L2_CID_BRIGHTNESS);
}

static void init(void) {
    struct v4l2_capability caps = {{0}};
    if (-1 == xioctl(VIDIOC_QUERYCAP, &caps, true)) {
        perror("Querying Capabilities");
        return;
    }
    
    // check if it is a capture dev
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        perror("No video capture device");
        state.quit = EINVAL;
        return;
    }
    
    // check if it does support streaming
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        perror("Device does not support streaming i/o");
        state.quit = EINVAL;
        return;
    }
    
    // check device priority level. No need to quit if this is not supported.
    enum v4l2_priority priority = V4L2_PRIORITY_BACKGROUND;
    if (-1 == xioctl(VIDIOC_S_PRIORITY, &priority, false)) {
        INFO("Failed to set priority\n");
    }
    
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 160;
    fmt.fmt.pix.height = 120;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    
    /* Check supported formats */
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (xioctl(VIDIOC_ENUM_FMT, &fmtdesc, false) == 0 && fmt.fmt.pix.pixelformat == 0) {    
        if (fmtdesc.pixelformat == V4L2_PIX_FMT_GREY) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        } else if (fmtdesc.pixelformat == V4L2_PIX_FMT_YUYV) {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        }
        fmtdesc.index++;
    }
    
    if (fmt.fmt.pix.pixelformat == 0) {
        perror("Device does not support neither GREY nor YUYV pixelformats.");
        state.quit = EINVAL;
        return;
    }
    
    INFO("Using %s pixelformat.\n", (char *)&fmt.fmt.pix.pixelformat);
    
    if (-1 == xioctl(VIDIOC_S_FMT, &fmt, true)) {
        perror("Setting Pixel Format");
        return;
    }    
    
    set_camera_settings();
    
    state.width = fmt.fmt.pix.width;
    state.height = fmt.fmt.pix.height;
    state.pixelformat = fmt.fmt.pix.pixelformat;
}

static void init_mmap(void) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req, true)) {
        perror("Requesting Buffer");
        return;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (-1 == xioctl(VIDIOC_QUERYBUF, &buf, true)) {
        perror("Querying Buffer");
        return;
    }
        
    state.buf.start = mmap(NULL,
                            buf.length,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            state.device_fd, buf.m.offset);
        
    if (MAP_FAILED == state.buf.start) {
        perror("mmap");
        state.quit = errno;
    } else {
        state.buf.length = buf.length;
    }
}

static int xioctl(int request, void *arg, bool exit_on_error) {
    int r;
    
    do {
        r = ioctl(state.device_fd, request, arg);
    } while (-1 == r && EINTR == errno);
    
    if (r == -1 && exit_on_error) {
        state.quit = errno;
    }
    return r;
}

static void start_stream(void) {
    /* 
     * Enqueue a buffer before start streaming,
     * as some driver would fail otherwise:
     * they require a buffer to be enqueued before start streaming.
     */
    send_frame();
    if (!state.quit) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(VIDIOC_STREAMON, &type, true)) {
            perror("Start Capture");
        }
    }
}

static void stop_stream(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(VIDIOC_STREAMOFF, &type, true)) {
        perror("Stop Capture");
    }
}

static void send_frame(void) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    /* Enqueue buffer */
    if (-1 == xioctl(VIDIOC_QBUF, &buf, true)) {
        perror("VIDIOC_QBUF");
    }
}

static void recv_frame(int i) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    /* Dequeue the buffer */
    if(-1 == xioctl(VIDIOC_DQBUF, &buf, true)) {
        perror("Retrieving Frame");
        return;
    }
    
    state.brightness[i] = compute_brightness(buf.bytesused) / CAMERA_ILL_MAX;
}

static double compute_brightness(const unsigned int size) {
    double brightness = 0.0;
    /*
     * If greyscale -> increment by 1. 
     * If YUYV -> increment by 2: we only want Y! 
     */
    const int inc = 1 + (state.pixelformat == V4L2_PIX_FMT_YUYV);
    
    for (int i = 0; i < size; i += inc) {
        brightness += state.buf.start[i];
    }
    brightness /= state.width * state.height;
    return brightness;
}

static void free_all(void) {
    if (state.buf.length) {
        munmap(state.buf.start, state.buf.length);
    }
    if (state.device_fd != -1) {
        close(state.device_fd);
    }
    /* reset state */
    memset(&state, 0, sizeof(struct state));
}
