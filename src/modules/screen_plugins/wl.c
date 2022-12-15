#include "screen.h"
#include "wl_utils.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"

struct cl_display {
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *shm;
    struct wl_list outputs;
    struct zwlr_screencopy_manager_v1 *screencopy_manager;
};

struct cl_buffer {
    struct wl_buffer *wl_buffer;
    void *shm_data;
};

struct cl_frame {
    enum wl_shm_format shm_format;
    int32_t width, height, stride, size;
    int brightness;
    bool copy_done;
    bool copy_err;
};

struct cl_output {
    struct wl_output *wl_output;
    struct cl_display *display;
    struct cl_buffer *buffer;
    struct cl_frame *frame;
    struct wl_list link;
    struct zwlr_screencopy_frame_v1 *screencopy_frame;
    char *name;
};

SCREEN("Wl");

void noop() {
}

struct wl_buffer *create_shm_buffer(struct wl_shm *shm,
                                    enum wl_shm_format format, int width,
                                    int height, int stride, int size, void **data_out) {
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
    struct wl_buffer *wl_buffer =
        wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_shm_pool_destroy(pool);
    *data_out = data;
    return wl_buffer;
}

static void frame_handle_buffer(void *data,
                                struct zwlr_screencopy_frame_v1 *frame,
                                enum wl_shm_format format, uint32_t width,
                                uint32_t height, uint32_t stride) {
    struct cl_output *output = (struct cl_output *)data;
    output->frame->shm_format = format;
    output->frame->width = width;
    output->frame->height = height;
    output->frame->stride = stride;
    output->frame->size = stride * height;
}

static void frame_handle_ready(void *data,
                               struct zwlr_screencopy_frame_v1 *frame,
                               uint32_t tv_sec_hi, uint32_t tv_sec_low,
                               uint32_t tv_nsec) {
    struct cl_output *output = (struct cl_output *)data;
    ++output->frame->copy_done;
}

static void frame_handle_failed(void *data,
                                struct zwlr_screencopy_frame_v1 *frame) {
    struct cl_output *output = (struct cl_output *)data;
    fprintf(stderr, "Failed to copy frame\n");
    ++output->frame->copy_err;
}

static void frame_handle_buffer_done(void *data,
                                     struct zwlr_screencopy_frame_v1 *frame) {
    struct cl_output *output = (struct cl_output *)data;
    struct cl_buffer *buffer = calloc(1, sizeof(struct cl_buffer));
    buffer->wl_buffer = create_shm_buffer(
        output->display->shm, output->frame->shm_format, output->frame->width,
        output->frame->height, output->frame->stride, output->frame->size,
        &buffer->shm_data);
    if (buffer->wl_buffer == NULL) {
        fprintf(stderr, "failed to create buffer\n");
        ++output->frame->copy_err;
    } else {
        output->buffer = buffer;
        zwlr_screencopy_frame_v1_copy(frame, output->buffer->wl_buffer);
    }
}

static const struct zwlr_screencopy_frame_v1_listener
    screencopy_frame_listener = {
        .buffer = frame_handle_buffer,
        .flags = noop,
        .ready = frame_handle_ready,
        .failed = frame_handle_failed,
        .buffer_done = frame_handle_buffer_done,
        .linux_dmabuf = noop,
};

static void output_handle_name(void *data, struct wl_output *wl_output,
                               const char *name) {
    struct cl_output *output = (struct cl_output *)data;
    if (output->name != NULL)
        free(output->name);
    output->name = strdup(name);
};

static const struct wl_output_listener wl_output_listener = {
    .name = output_handle_name,
    .geometry = noop,
    .mode = noop,
    .scale = noop,
    .description = noop,
    .done = noop,
};

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface,
                          uint32_t version) {
    struct cl_display *display = (struct cl_display *)data;
    struct cl_output *output = calloc(1, sizeof(struct cl_output));
    output->display = display;
    if (strcmp(interface, wl_output_interface.name) == 0) {
        output->wl_output =
            wl_registry_bind(registry, name, &wl_output_interface, 4);
        wl_output_add_listener(output->wl_output, &wl_output_listener, output);
        output->name = NULL;
        wl_list_insert(&display->outputs, &output->link);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        display->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) ==
               0) {
        display->screencopy_manager = wl_registry_bind(
            registry, name, &zwlr_screencopy_manager_v1_interface, 3);
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = noop,
};

static void wl_cleanup(struct cl_display *display) {
    struct cl_output *output;
    struct cl_output *tmp_output;
    wl_list_for_each_safe(output, tmp_output, &display->outputs, link) {
        output->frame->copy_done = false;
        output->frame->copy_err = false;
        if (output->screencopy_frame != NULL) {
            zwlr_screencopy_frame_v1_destroy(output->screencopy_frame);
        }
        if (output->buffer->shm_data) {
            munmap(output->buffer->shm_data, output->frame->size);
        }
        if (output->buffer->wl_buffer) {
            wl_buffer_destroy(output->buffer->wl_buffer);
        }
        free(output->buffer);
        wl_output_destroy(output->wl_output);
        wl_list_remove(&output->link);
        free(output->name);
        free(output);
    }
    if (display->wl_registry) {
        wl_registry_destroy(display->wl_registry);
    }
    if (display->screencopy_manager) {
        zwlr_screencopy_manager_v1_destroy(display->screencopy_manager);
    }
}

static int get_frame_brightness(const char *id, const char *env) {
    struct cl_display display = {};
    struct cl_output *output;
    int ret = 0;

    display.wl_display = fetch_wl_display(id, env);

    if (display.wl_display == NULL) {
        fprintf(stderr, "display error\n");
        ret = WRONG_PLUGIN;
        goto err;
    }
    wl_list_init(&display.outputs);

    display.wl_registry = wl_display_get_registry(display.wl_display);
    wl_registry_add_listener(display.wl_registry, &registry_listener, &display);
    wl_display_roundtrip(display.wl_display);
    if (display.shm == NULL) {
        fprintf(stderr, "Compositor is missing wl_shm\n");
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    if (display.screencopy_manager == NULL) {
        fprintf(stderr, "Compositor is screencopy manager\n");
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    if (wl_list_empty(&display.outputs)) {
        fprintf(stderr, "No outputs available\n");
        ret = UNSUPPORTED;
        goto err;
    }
    int sum = 0;
    wl_list_for_each(output, &display.outputs, link) {
        struct cl_frame *frame = calloc(1, sizeof(struct cl_frame));
        output->frame = frame;
        output->screencopy_frame = zwlr_screencopy_manager_v1_capture_output(
            display.screencopy_manager, 0, output->wl_output);
        zwlr_screencopy_frame_v1_add_listener(
            output->screencopy_frame, &screencopy_frame_listener, output);

        while (!output->frame->copy_done && !output->frame->copy_err &&
               wl_display_dispatch(display.wl_display) != -1) {
            // This space is intentionally left blank
        }
        if (output->frame->copy_done) {
            output->frame->brightness = rgb_frame_brightness(
                output->buffer->shm_data, output->frame->width,
                output->frame->height, output->frame->stride);
            sum += output->frame->brightness;
        }
    }
    if (sum > 0) {
        sum = sum / wl_list_length(&display.outputs);
    }
err:
    wl_cleanup(&display);
    if (ret != 0) {
        return ret;
    }
    // NOTE: dpy is disconnected on program exit to workaround
    // gamma protocol limitation that resets gamma as soon as display is disconnected.
    // See wl_utils.c
    return sum;
}
