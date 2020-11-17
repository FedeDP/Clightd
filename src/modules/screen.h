#include "commons.h"

#define _SCREEN_PLUGINS \
    X(XORG, 0) \
    X(WL, 1) \
    X(FB, 2)

enum screen_plugins { 
#define X(name, val) name = val,
    _SCREEN_PLUGINS
#undef X
    SCREEN_NUM
};

typedef struct {
    const char *name;
    int (*get)(const char *id, const char *env);
    char obj_path[100];
} screen_plugin;

#define SCREEN(name) \
    static int get_frame_brightness(const char *id, const char *env); \
    static void _ctor_ register_gamma_plugin(void) { \
        static screen_plugin self = { name, get_frame_brightness }; \
        screen_register_new(&self); \
    }

void screen_register_new(screen_plugin *plugin);
int rgb_frame_brightness(const uint8_t *data, const int width, const int height, const int stride);
