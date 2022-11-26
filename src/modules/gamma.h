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

struct _gamma_plugin;

typedef struct _gamma_cl {
    unsigned int target_temp;
    bool is_smooth;
    unsigned int smooth_step;
    unsigned int smooth_wait;
    unsigned int current_temp;
    const char *display;
    const char *env;
    int fd;
    struct _gamma_plugin *plugin;
    void *priv;
} gamma_client;

/* 
 * validate() takes double pointer as 
 * drm plugin may override id.
 */
typedef struct _gamma_plugin {
    const char *name;
    int (*validate)(const char **id, const char *env, void **priv_data);
    int (*set)(void *priv_data, const int temp);
    int (*get)(void *priv_data);
    void (*dtor)(void *priv_data);
    char obj_path[100];
} gamma_plugin;

#define GAMMA(name) \
    static int validate(const char **id, const char *env, void **priv_data); \
    static int set(void *priv_data, const int temp); \
    static int get(void *priv_data); \
    static void dtor(void *priv_data); \
    static void _ctor_ register_gamma_plugin(void) { \
        static gamma_plugin self = { name, validate, set, get, dtor }; \
        gamma_register_new(&self); \
    }

void gamma_register_new(gamma_plugin *plugin);
double clamp(double x, double min, double max);
int get_temp(const unsigned short R, const unsigned short B);
void fill_gamma_table(uint16_t *r, uint16_t *g, uint16_t *b, double br, uint32_t ramp_size, int temp);

/* Gamma brightness related (ie: emulated backlight) */
void store_gamma_brightness(const char *id, double brightness);
double fetch_gamma_brightness(const char *id);
void clean_gamma_brightness(const char *id);
int refresh_gamma(void);
