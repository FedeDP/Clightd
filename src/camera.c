#include "../inc/camera.h"
#include "../inc/polkit.h"
#include "../inc/udev.h"
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define CAMERA_SUBSYSTEM        "video4linux"

static void capture_frames(const char *interface, int num_captures, int *err);
static void open_device(const char *interface);
static void init(void);
static void init_mmap(void);
static int xioctl(int request, void *arg);
static void start_stream(void);
static void stop_stream(void);
static void capture_frame(int i);
static double compute_brightness(const int idx, const unsigned int size);
static void free_all();

struct buffer {
    uint8_t *start;
    size_t length;
};

struct state {
    int quit, width, height, device_fd, num_captures;
    double *brightness_values;
    struct buffer *buffers;
};

static struct state state;

int method_iswebcamavailable(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct udev_device *dev = NULL;
    int present = 0;
    
    get_udev_device(NULL, CAMERA_SUBSYSTEM, NULL, &dev);
    if (dev) {
        present = 1;
        udev_device_unref(dev);
    }
    return sd_bus_reply_method_return(m, "b", present);
}

/*
 * Frame capturing method
 */
int method_captureframes(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int r, error = 0, num_captures;
    struct udev_device *dev = NULL;
    const char *video_interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "si", &video_interface, &num_captures);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (num_captures <= 0 || num_captures > 20) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Number of captures should be between 1 and 20.");
        return -EINVAL;
    }
    
    // if no video device is specified, try to get first matching device
    get_udev_device(video_interface, CAMERA_SUBSYSTEM, &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    capture_frames(udev_device_get_devnode(dev), num_captures, &error);
    if (error) {
        sd_bus_error_set_errno(ret_error, error);
        r = -error;
        goto end;
    }

    printf("%d frames captured by %s.\n", num_captures, udev_device_get_sysname(dev));
    
    /* Reply with array response */
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_append_array(reply, 'd', state.brightness_values, num_captures * sizeof(double));
    r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    
end:
    udev_device_unref(dev);
    free_all();
    return r;
}

static void capture_frames(const char *interface, int num_captures, int *err) {
    state.num_captures = num_captures;
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

        state.brightness_values = calloc(num_captures, sizeof(double));

        start_stream();
        if (state.quit) {
            goto end;
        }
        for (int i = 0; i < num_captures && !state.quit; i++) {
            capture_frame(i);
        }
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
    req.count = state.num_captures;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req)) {
        perror("Requesting Buffer");
        return;
    }
    
    state.buffers = calloc(req.count, sizeof(struct buffer));
    if (!state.buffers) {
        state.quit = 1;
        perror("State->buffers");
        return;
    }
    
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(-1 == xioctl(VIDIOC_QUERYBUF, &buf)) {
            perror("Querying Buffer");
            return;
        }
    
        state.buffers[i].start = mmap(NULL /* start anywhere */,
                                buf.length,
                                PROT_READ | PROT_WRITE /* required */,
                                MAP_SHARED /* recommended */,
                                state.device_fd, buf.m.offset);
        
        if (MAP_FAILED == state.buffers[i].start) {
            perror("mmap");
            state.quit = 1;
            break;
        }
        state.buffers[i].length = buf.length;
    }
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
    for (int i = 0; i < state.num_captures; i++) {
        struct v4l2_buffer buf = {0};
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (-1 == xioctl(VIDIOC_QBUF, &buf)) {
            perror("VIDIOC_QBUF");
        }
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

static void capture_frame(int i) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // dequeue the buffer
    if(-1 == xioctl(VIDIOC_DQBUF, &buf)) {
        perror("Retrieving Frame");
        return;
    }
    
    state.brightness_values[i] = compute_brightness(i, buf.bytesused);
    
    // query a buffer from camera if needed
    if (i < state.num_captures - 1) {
        if(-1 == xioctl(VIDIOC_QBUF, &buf)) {
            perror("Query Buffer");
            return stop_stream();
        }
    }
}

static double compute_brightness(const int idx, const unsigned int size) {
    double brightness = 0.0;
    
    for (int i = 0; i < size; i += 2) {
        brightness += (unsigned int) (state.buffers[idx].start[i]);
    }
    brightness /= state.width * state.height;
    return brightness;
}

static void free_all(void) {
    if (state.buffers) {
        for (int i = 0; i < state.num_captures; i++) {
            munmap(state.buffers[i].start, state.buffers[i].length);
        }
        free(state.buffers);
    }
    if (state.device_fd != -1) {
        close(state.device_fd);
    }
    if (state.brightness_values) {
        free(state.brightness_values);
    }
    /* reset state */
    memset(&state, 0, sizeof(struct state));
}
