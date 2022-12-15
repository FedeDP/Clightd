#pragma once

#include "wlr-output-management-unstable-v1-client-protocol.h"
#include <stdbool.h>
#include <sys/mman.h>
#ifdef SCREEN_PRESENT
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "wlr-export-dmabuf-unstable-v1-client-protocol.h"
#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include <assert.h>
#include <xf86drm.h>
#endif

struct wl_display *fetch_wl_display(const char *display, const char *env);
int create_anonymous_file(off_t size, const char *filename);
void noop();

struct cl_display {
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct zwlr_output_manager_v1 *output_manager;
    struct wl_list outputs;
#ifdef SCREEN_PRESENT
    struct zwp_linux_dmabuf_v1 *dmabuf;
    struct zwp_linux_dmabuf_feedback_v1 *dmabuf_feedback;
    struct zwlr_export_dmabuf_manager_v1 *dmabuf_manager;
    struct wl_shm *shm;
    struct zwlr_screencopy_manager_v1 *screencopy_manager;
    bool have_linux_dmabuf;
    bool try_linux_dmabuf;
#endif
};

#define WLR_DMABUF_MAX_PLANES 4

struct cl_buffer {
    struct wl_buffer *wl_buffer;
#ifdef SCREEN_PRESENT
    struct zwp_linux_buffer_params_v1 *params;
    void *shm_data;
    uint8_t *dma_data[WLR_DMABUF_MAX_PLANES];
    uint32_t sizes[WLR_DMABUF_MAX_PLANES];
    uint32_t strides[WLR_DMABUF_MAX_PLANES];
    uint32_t offsets[WLR_DMABUF_MAX_PLANES];
    int fds[WLR_DMABUF_MAX_PLANES];
#endif
};

struct cl_frame {
#ifdef SCREEN_PRESENT
    int32_t width, height, stride, size, buf_flags, dma_format;
    enum wl_shm_format shm_format;
    uint32_t mod_hi;
    uint32_t mod_lo;
    uint32_t num_objects;

    int brightness;

    bool copy_done;
    bool copy_err;
#endif
};

struct cl_output {
    struct cl_display *display;
    struct wl_output *wl_output;
    struct wl_list link;
    char *name;

    struct cl_buffer *buffer;

#ifdef SCREEN_PRESENT
    dev_t main_device_id;
    struct cl_frame *frame;
    struct zwp_linux_buffer_params_v1 *params;
    struct zwlr_export_dmabuf_frame_v1 *dmabuf_frame;
    struct zwlr_screencopy_frame_v1 *screencopy_frame;
#endif
};
