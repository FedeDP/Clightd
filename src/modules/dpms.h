#include "commons.h"

#define _DPMS_PLUGINS \
    X(XORG, 0) \
    X(WL, 1) \
    X(DRM, 2)

enum dpms_plugins { 
#define X(name, val) name = val,
    _DPMS_PLUGINS
#undef X
    DPMS_NUM
};

typedef struct {
    const char *name;
    int (*set)(const char *id, const char *env, int level);
    int (*get)(const char *id, const char *env);
    char obj_path[100];
} dpms_plugin;

#define DPMS(name) \
    static int get(const char *id, const char *env); \
    static int set(const char *id, const char *env, int level); \
    static void _ctor_ register_gamma_plugin(void) { \
        static dpms_plugin self = { name, set, get }; \
        dpms_register_new(&self); \
    }

void dpms_register_new(dpms_plugin *plugin);
