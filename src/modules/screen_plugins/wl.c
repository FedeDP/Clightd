#include "screen.h"
#include "wl_utils.h"

SCREEN("Wl");

struct cl_buffer *create_shm_buffer(struct wl_shm *shm,
                                    enum wl_shm_format format, int width,
                                    int height, int stride, int size) {
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
    struct wl_buffer *wl_buffer =
        wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_shm_pool_destroy(pool);
    close(fd);
    struct cl_buffer *buffer = calloc(1, sizeof(struct cl_buffer));
    buffer->wl_buffer = wl_buffer;
    buffer->shm_data = data;
    return buffer;
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
    output->buffer = create_shm_buffer(
        output->display->shm, output->frame->shm_format, output->frame->width,
        output->frame->height, output->frame->stride, output->frame->size);
    if (output->buffer == NULL) {
        fprintf(stderr, "failed to create buffer\n");
        ++output->frame->copy_err;
    } else {
        zwlr_screencopy_frame_v1_copy(output->screencopy_frame,
                                      output->buffer->wl_buffer);
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
static void frame_start(void *data,
                        struct zwlr_export_dmabuf_frame_v1 *zwlr_frame,
                        uint32_t width, uint32_t height, uint32_t offset_x,
                        uint32_t offset_y, uint32_t buffer_flags,
                        uint32_t flags, uint32_t format, uint32_t mod_high,
                        uint32_t mod_low, uint32_t num_objects) {
    struct cl_output *output = (struct cl_output *)data;
    struct cl_buffer *buffer = calloc(1, sizeof(struct cl_buffer));
    output->params = zwp_linux_dmabuf_v1_create_params(output->display->dmabuf);
    output->buffer = buffer;
    output->frame->width = width;
    output->frame->height = height;
    output->frame->buf_flags = flags;
    output->frame->dma_format = format;
    output->frame->mod_hi = mod_high;
    output->frame->mod_lo = mod_low;
    output->frame->num_objects = num_objects;
}
static void frame_object(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                         uint32_t index, int32_t fd, uint32_t size,
                         uint32_t offset, uint32_t stride,
                         uint32_t plane_index) {
    struct cl_output *output = (struct cl_output *)data;
    output->buffer->fds[plane_index] = fd;
    output->buffer->sizes[plane_index] = size;
    output->buffer->offsets[plane_index] = offset;
    output->buffer->strides[plane_index] = stride;
    void *dma_data = mmap(NULL, size, PROT_READ | PROT_WRITE,
                          output->frame->buf_flags, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: \n");
        close(fd);
    }
    output->buffer->dma_data[plane_index] = dma_data;
}
static void frame_ready(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                        uint32_t tv_sec_hi, uint32_t tv_sec_lo,
                        uint32_t tv_nsec) {
    struct cl_output *output = (struct cl_output *)data;
    for (int i = 0; i < output->frame->num_objects; ++i) {
        zwp_linux_buffer_params_v1_add(
            output->params, output->buffer->fds[i], i,
            output->buffer->offsets[i], output->buffer->strides[i],
            output->frame->mod_hi, output->frame->mod_lo);
    }
    output->buffer->wl_buffer = zwp_linux_buffer_params_v1_create_immed(
        output->params, output->frame->width, output->frame->height,
        output->frame->dma_format, 0);
    zwp_linux_buffer_params_v1_destroy(output->params);
    ++output->frame->copy_done;
}
static void frame_cancel(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                         uint32_t reason) {
    struct cl_output *output = (struct cl_output *)data;
    ++output->frame->copy_err;
}
static const struct zwlr_export_dmabuf_frame_v1_listener dmabuf_frame_listener =
    {
        .frame = frame_start,
        .object = frame_object,
        .ready = frame_ready,
        .cancel = frame_cancel,
};
static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
    .format = noop,
    .modifier = noop,
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
    } else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
        display->dmabuf =
            wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, 4);
    } else if (strcmp(interface,
                      zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
        display->dmabuf_manager = wl_registry_bind(
            registry, name, &zwlr_export_dmabuf_manager_v1_interface, 1);
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
        if (output->dmabuf_frame != NULL) {
            zwlr_export_dmabuf_frame_v1_destroy(output->dmabuf_frame);
        }
        if (output->screencopy_frame != NULL) {
            zwlr_screencopy_frame_v1_destroy(output->screencopy_frame);
        }
        if (output->buffer->dma_data[0]) {
            for (int i = 0; i < output->frame->num_objects; ++i) {
                if (output->buffer->dma_data[i]) {
                    munmap(output->buffer->dma_data[i],
                           output->buffer->sizes[i]);
                }
            }
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
    if (display->dmabuf_manager) {
        zwlr_export_dmabuf_manager_v1_destroy(display->dmabuf_manager);
    }
    if (display->screencopy_manager) {
        zwlr_screencopy_manager_v1_destroy(display->screencopy_manager);
    }
    wl_display_flush(display->wl_display);
    wl_display_disconnect(display->wl_display);
}
static int get_frame_brightness(const char *id, const char *env) {
    struct cl_display display = {};
    struct cl_output *output;
    int ret = 0;

    char *socket_file;
    int len = snprintf(NULL, 0, "%s/%s", env, id);
    if (len < 0) {
        perror("snprintf failed");
        return EXIT_FAILURE;
    }
    socket_file = malloc(len + 1);
    snprintf(socket_file, len + 1, "%s/%s", env, id);
    /* fprintf(stderr, "Using wayland socket: %s\n", socket_file); */
    display.wl_display = wl_display_connect(socket_file);
    free(socket_file);
    if (display.wl_display == NULL) {
        fprintf(stderr, "display error\n");
        ret = WRONG_PLUGIN;
        goto err;
    }
    wl_list_init(&display.outputs);
    display.wl_registry = wl_display_get_registry(display.wl_display);
    wl_registry_add_listener(display.wl_registry, &registry_listener, &display);
    wl_display_roundtrip(display.wl_display);
    wl_display_roundtrip(display.wl_display);

    if (!display.dmabuf && display.shm == NULL) {
        fprintf(stderr, "Compositor is missing wl_shm\n");
        ret = COMPOSITOR_NO_PROTOCOL;
        goto err;
    }
    if (!display.dmabuf && display.screencopy_manager == NULL) {
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
        if (display.dmabuf) {
            output->dmabuf_frame = zwlr_export_dmabuf_manager_v1_capture_output(
                display.dmabuf_manager, 0, output->wl_output);
            zwlr_export_dmabuf_frame_v1_add_listener(
                output->dmabuf_frame, &dmabuf_frame_listener, output);
        } else {
            output->screencopy_frame =
                zwlr_screencopy_manager_v1_capture_output(
                    display.screencopy_manager, 0, output->wl_output);
            zwlr_screencopy_frame_v1_add_listener(
                output->screencopy_frame, &screencopy_frame_listener, output);
        }
        while (!output->frame->copy_done && !output->frame->copy_err &&
               wl_display_dispatch(display.wl_display) != -1) {
            // This space is intentionally left blank
        }
        if (output->frame->copy_done) {
            if (output->buffer->dma_data[0]) {
                /* fprintf(stderr, "using dma buffers\n"); */
                output->frame->brightness = rgb_frame_brightness(
                    output->buffer->dma_data[0], output->frame->width,
                    output->frame->height, output->buffer->strides[0]);
            } else {
                /* fprintf(stderr, "using shm buffers\n"); */
                output->frame->brightness = rgb_frame_brightness(
                    output->buffer->shm_data, output->frame->width,
                    output->frame->height, output->frame->stride);
            }
            /* fprintf(stderr, "Output: %s Brightness: %d Percent: %.02f%%\n",
             */
            /*         output->name, output->frame->brightness, */
            /*         (output->frame->brightness / 255.0) * 100.0); */
            sum += output->frame->brightness;
        }
        if (output->frame->num_objects) {
            for (int i = 0; i < output->frame->num_objects; ++i) {
                close(output->buffer->fds[i]);
                output->buffer->fds[i] = -1;
            }
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
    return sum;
}
