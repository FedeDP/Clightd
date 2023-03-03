#pragma once

#include "commons.h"
#include <module/map.h>

#define _BL_PLUGINS \
    X(SYSFS, 0) \
    X(DDC, 1) \

enum backlight_plugins { 
    #define X(name, val) name = val,
    _BL_PLUGINS
    #undef X
    BL_NUM
};

typedef struct {
    double target_pct;
    double step;
    unsigned int wait;
} smooth_params_t;

typedef struct {
    smooth_params_t params;
    int fd;
} smooth_t;

struct _bl_plugin;

typedef struct bl {
    bool is_internal;
    bool is_ddc;
    bool is_emulated;
    void *dev; // differs between struct udev_device (internal devices) and DDCA_Display_Ref for external ones
    int max; // cached device max backlight value
    char obj_path[100];
    const char *sn;
    sd_bus_slot *slot;
    smooth_t *smooth; // when != NULL -> smoothing
    uint64_t cookie;
    struct _bl_plugin *plugin;
} bl_t;

typedef struct _bl_plugin {
    const char *name;
    bool (*load_env)(void); // returns true to actually enable the plugin
    void (*load_devices)(void);
    int (*get_monitor)(void);
    void (*receive)(void);
    int (*set)(bl_t *dev, int value);
    int (*get)(bl_t *dev);
    void (*free_device)(bl_t *dev);
    void (*dtor)(void);
} bl_plugin;

#define BACKLIGHT(name) \
    static bool load_env(void); \
    static void load_devices(void); \
    static int get_monitor(void); \
    static void receive(void); \
    static int set(bl_t *dev, int value); \
    static int get(bl_t *dev); \
    static void free_device(bl_t *dev); \
    static void dtor(void); \
    static void _ctor_ register_backlight_plugin(void) { \
        static bl_plugin self = { name, load_env, load_devices, get_monitor, receive, set, get, free_device, dtor }; \
        bl_register_new(&self); \
    }

void bl_register_new(bl_plugin *plugin);
int store_device(bl_t *bl, enum backlight_plugins plenum);
void emit_signals(bl_t *bl, double pct);

extern map_t *bls;
