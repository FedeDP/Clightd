#include "screen.h"
#include "wl_utils.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

static struct wl_buffer *create_shm_buffer(enum wl_shm_format fmt,
        int width, int height, int stride, void **data_out);
static void frame_handle_buffer(void *tt, struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
    uint32_t width, uint32_t height, uint32_t stride);
static void frame_handle_flags(void*tt, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags);
static void frame_handle_ready(void *tt, struct zwlr_screencopy_frame_v1 *frame,
    uint32_t tv_sec_hi, uint32_t tv_sec_low, uint32_t tv_nsec);
static void frame_handle_failed(void *tt, struct zwlr_screencopy_frame_v1 *frame);
static void handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version);
static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name);
static void dtor(void);

static struct {
    struct wl_buffer *wl_buffer;
    void *data;
    enum wl_shm_format format;
    int width, height, stride;
    bool y_invert;
} buffer;

SCREEN("Wl");

static struct zwlr_screencopy_manager_v1 *screencopy_manager;
static struct wl_output *output;
static struct wl_shm *shm;
static struct wl_registry *registry;
static struct zwlr_screencopy_frame_v1 *frame;
static bool buffer_copy_done;
static bool buffer_copy_err;

static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .buffer = frame_handle_buffer,
    .flags = frame_handle_flags,
    .ready = frame_handle_ready,
    .failed = frame_handle_failed,
};

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static int get_frame_brightness(const char *id, const char *env) {
    struct wl_display *display = fetch_wl_display(id, env);
    if (display == NULL) {
        return WRONG_PLUGIN;
    }
    
    int ret = UNSUPPORTED;

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    
    if (screencopy_manager == NULL) {
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    if (shm == NULL) {
        fprintf(stderr, "compositor is missing wl_shm\n");
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    if (output == NULL) {
        fprintf(stderr, "no outputs available\n");
        goto err;
    }
    
    frame = zwlr_screencopy_manager_v1_capture_output(screencopy_manager, 0, output);
    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, NULL);
    
    while (!buffer_copy_done && !buffer_copy_err && wl_display_dispatch(display) != -1) {
        // This space is intentionally left blank
    }
    
    ret = -EIO;
    if (buffer_copy_done) {
        ret = rgb_frame_brightness(buffer.data, buffer.width, buffer.height, buffer.stride);
    }
    
err:
    dtor();
    return ret;
}

static struct wl_buffer *create_shm_buffer(enum wl_shm_format fmt,
        int width, int height, int stride, void **data_out) {
    
    const int size = stride * height;

    int fd = create_anonymous_file(size, "clightd-screen-wlr");
    if (fd < 0) {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
        return NULL;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    close(fd);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
        stride, fmt);
    wl_shm_pool_destroy(pool);

    *data_out = data;
    return buffer;
}

static void frame_handle_buffer(void *tt, struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
    uint32_t width, uint32_t height, uint32_t stride) {

    buffer.format = format;
    buffer.width = width;
    buffer.height = height;
    buffer.stride = stride;
    buffer.wl_buffer = create_shm_buffer(format, width, height, stride, &buffer.data);
    if (buffer.wl_buffer == NULL) {
        fprintf(stderr, "failed to create buffer\n");
        buffer_copy_err = true;
    } else {
        zwlr_screencopy_frame_v1_copy(frame, buffer.wl_buffer);
    }
}

static void frame_handle_flags(void*tt, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
    buffer.y_invert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

static void frame_handle_ready(void *tt, struct zwlr_screencopy_frame_v1 *frame,
    uint32_t tv_sec_hi, uint32_t tv_sec_low, uint32_t tv_nsec) {
    buffer_copy_done = true;
}

static void frame_handle_failed(void *tt, struct zwlr_screencopy_frame_v1 *frame) {
    fprintf(stderr, "failed to copy frame\n");
    buffer_copy_err = true;
}

static void handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {

    // we just use first output
    if (strcmp(interface, wl_output_interface.name) == 0 && !output) {
        output = wl_registry_bind(registry, name, &wl_output_interface, 1);
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        screencopy_manager = wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, 2);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    
}

static void dtor(void) {
    buffer_copy_done = false;
    buffer_copy_err = false;
    
    /* Free everything */
    if (buffer.wl_buffer) {
        wl_buffer_destroy(buffer.wl_buffer);
        buffer.wl_buffer = NULL;
    }
    if (buffer.data) {
        munmap(buffer.data, (size_t)buffer.height * buffer.stride);
        buffer.data = NULL;
    }
    if (output) {
        wl_output_destroy(output);
        output = NULL;
    }
     if (registry) {
        wl_registry_destroy(registry);
    }
    if (frame) {
        zwlr_screencopy_frame_v1_destroy(frame);
    }
    if (shm) {
        wl_shm_destroy(shm);
    }
    if (screencopy_manager) {
        zwlr_screencopy_manager_v1_destroy(screencopy_manager);
    }
     // NOTE: dpy is disconnected on program exit to workaround
    // gamma protocol limitation that resets gamma as soon as display is disconnected.
    // See wl_utils.c
}
