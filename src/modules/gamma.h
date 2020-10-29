#pragma once

#include "commons.h"

struct _gamma_cl;

#define _GAMMA_PLUGINS \
    X(XORG, 0) \
    X(WL, 1) \
    X(DRM, 2)

enum gamma_plugins { 
#define X(name, val) name = val,
    _GAMMA_PLUGINS
#undef X
    GAMMA_NUM
};

typedef struct {
    int (*set)(struct _gamma_cl *cl, const int temp);
    int (*get)(struct _gamma_cl *cl);
    int (*dtor)(struct _gamma_cl *cl);
    void (*validate)(struct _gamma_cl *cl); 
    void *priv;
} gamma_handler;

typedef struct _gamma_cl {
    unsigned int target_temp;
    bool is_smooth;
    unsigned int smooth_step;
    unsigned int smooth_wait;
    unsigned int current_temp;
    char *display;
    char *env;
    int fd;
    gamma_handler handler;
} gamma_client;

typedef struct {
    const char *name;
    int (*get_handler)(gamma_client *cl);
    char obj_path[100];
} gamma_plugin;

#define GAMMA(name) \
    static int get_handler(gamma_client *cl); \
    static void _ctor_ register_gamma_plugin(void) { \
        static gamma_plugin self = { name, get_handler }; \
        gamma_register_new(&self); \
    }

void gamma_register_new(gamma_plugin *plugin);
double clamp(double x, double min, double max);
int get_temp(const unsigned short R, const unsigned short B);
void fill_gamma_table(uint16_t *r, uint16_t *g, uint16_t *b, uint32_t ramp_size, int temp);
