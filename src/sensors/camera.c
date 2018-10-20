#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <sensor.h>

#define CAMERA_SUBSYSTEM        "video4linux"

static void recv_frame(const char *interface, int *err);
static void open_device(const char *interface);
static void init(void);
static void init_mmap(void);
static int xioctl(int request, void *arg);
static void start_stream(void);
static void stop_stream(void);
static void capture_frame(void);
static double compute_brightness(const unsigned int size);
static void free_all();

struct buffer {
    uint8_t *start;
    size_t length;
};

struct state {
    int quit, width, height, device_fd;
    double brightness;
    struct buffer buf;
};

static struct state state;

SENSOR("webcam", CAMERA_SUBSYSTEM, NULL);

/*
 * Frame capturing method
 */
static int capture(struct udev_device *dev, double *pct) {
    int r = 0;
    /* 
     * Only single frame capturing is supported now for the bus api.
     * Leaving num_captures parameter here for future reference 
     */
    recv_frame(udev_device_get_devnode(dev), &r);
    if (!r) {
        *pct = state.brightness;
    }
    free_all();
    return -r;
}

static void recv_frame(const char *interface, int *err) {
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
        capture_frame();
        stop_stream();
    }
    
end:
    if (state.quit) {
        *err = state.quit;
    }
}

static void open_device(const char *interface) {
    state.device_fd = open(interface, O_RDWR);
    if (state.device_fd == -1) {
        perror(interface);
        state.quit = 1;
    }
}

static void init(void) {
    struct v4l2_capability caps = {{0}};
    if (-1 == xioctl(VIDIOC_QUERYCAP, &caps)) {
        perror("Querying Capabilities");
        return;
    }
    
    // check if it is a capture dev
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "No video capture device\n");
        state.quit = 1;
        return;
    }
    
    // check if it does support streaming
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming i/o\n");
        state.quit = 1;
        return;
    }
    
    // check device priority level. Do not need to quit if this is not supported.
    enum v4l2_priority priority = V4L2_PRIORITY_BACKGROUND;
    if (-1 == xioctl(VIDIOC_S_PRIORITY, &priority)) {
        perror("Setting priority");
        state.quit = 0;
    }
    
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 160;
    fmt.fmt.pix.height = 120;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (-1 == xioctl(VIDIOC_S_FMT, &fmt)) {
        perror("Setting Pixel Format");
        return;
    }
    
    struct v4l2_control ctrl ={0};
    // disable auto white balance -- DONT LEAVE IF UNSUPPORTED
    ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
    if (-1 == xioctl(VIDIOC_S_CTRL, &ctrl)) {
        perror("Setting V4L2_CID_AUTO_WHITE_BALANCE");
        state.quit = 0;
    }
    
    memset(&ctrl, 0, sizeof(struct v4l2_control));
    // disable autogain -- DONT LEAVE IF UNSUPPORTED
    ctrl.id = V4L2_CID_AUTOGAIN;
    if (-1 == xioctl(VIDIOC_S_CTRL, &ctrl)) {
        perror("setting V4L2_CID_AUTOGAIN");
        state.quit = 0;
    }
    
    memset(&ctrl, 0, sizeof(struct v4l2_control));
    // disable backlight compensation -- DONT LEAVE IF UNSUPPORTED
    ctrl.id = V4L2_CID_BACKLIGHT_COMPENSATION;
    if (-1 == xioctl(VIDIOC_S_CTRL, &ctrl)) {
        perror("setting V4L2_CID_BACKLIGHT_COMPENSATION");
        state.quit = 0;
    }
    
    state.width = fmt.fmt.pix.width;
    state.height = fmt.fmt.pix.height;
}

static void init_mmap(void) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req)) {
        perror("Requesting Buffer");
        return;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(-1 == xioctl(VIDIOC_QUERYBUF, &buf)) {
        perror("Querying Buffer");
        return;
    }
    
    state.buf.start = mmap(NULL /* start anywhere */,
                            buf.length,
                            PROT_READ | PROT_WRITE /* required */,
                            MAP_SHARED /* recommended */,
                            state.device_fd, buf.m.offset);
        
    if (MAP_FAILED == state.buf.start) {
        perror("mmap");
        state.quit = 1;
    }
    state.buf.length = buf.length;
}

static int xioctl(int request, void *arg) {
    int r;
    
    do {
        r = ioctl(state.device_fd, request, arg);
    } while (-1 == r && EINTR == errno);
    
    if (r == -1) {
        state.quit = errno;
    }
    return r;
}

static void start_stream(void) {
    struct v4l2_buffer buf = {0};
        
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
        
    if (-1 == xioctl(VIDIOC_QBUF, &buf)) {
        perror("VIDIOC_QBUF");
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(VIDIOC_STREAMON, &type)) {
        perror("Start Capture");
    }
}

static void stop_stream(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(VIDIOC_STREAMOFF, &type)) {
        perror("Stop Capture");
    }
}

static void capture_frame(void) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // dequeue the buffer
    if(-1 == xioctl(VIDIOC_DQBUF, &buf)) {
        perror("Retrieving Frame");
        return;
    }
    
    state.brightness = compute_brightness(buf.bytesused);
}

static double compute_brightness(const unsigned int size) {
    double brightness = 0.0;
    
    for (int i = 0; i < size; i += 2) {
        brightness += (unsigned int) (state.buf.start[i]);
    }
    brightness /= state.width * state.height;
    return brightness;
}

static void free_all(void) {
    munmap(state.buf.start, state.buf.length);
    if (state.device_fd != -1) {
        close(state.device_fd);
    }
    /* reset state */
    memset(&state, 0, sizeof(struct state));
}
