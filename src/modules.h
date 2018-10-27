#include <commons.h>

#define MODULE(module) \
    static int init(void); \
    static void destroy(void); \
    static int callback(const int fd); \
    static module_t *self; \
    static void _ctor0_ set_module_self(void) { \
        *((int *)&modules[module].idx) = module; \
        modules[module].name = #module; \
        modules[module].init = init; \
        modules[module].poll_cb = callback; \
        modules[module].destroy = destroy; \
        self = &modules[module]; \
        MODULE_INFO("Registered module.\n"); \
    }

#define REGISTER_FD(fd)                  register_fd(fd, self)
#define GET_FD()                         moduled_get_fd(self)
#define MODULE_PRINTF(out, type, ...)    fprintf(out, "%s -> %c| ", self->name, type); fprintf(out, __VA_ARGS__)
#define MODULE_INFO(...)                 MODULE_PRINTF(stdout, 'I', __VA_ARGS__)
#define MODULE_ERR(...)                  MODULE_PRINTF(stderr, 'E', __VA_ARGS__)

int register_fd(int fd, module_t *self);
int init_modules(void);
int loop_modules(void);
void destroy_modules(void);
int moduled_get_fd(module_t *self);
