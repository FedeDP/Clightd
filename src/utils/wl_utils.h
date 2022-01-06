#pragma once

#include <wayland-client.h>
#include <sys/mman.h>

struct wl_display *fetch_wl_display(const char *display, const char *env);
int create_anonymous_file(off_t size, const char *filename);
