#include <modules.h>
#include <poll.h>

static int poll_cb(const int idx);

static int *fds_to_module;
static struct pollfd *main_p;
static int num_fds;
static int quit;

int register_fd(int fd, module_t *self) {
    struct pollfd *ptmp = realloc(main_p, sizeof(struct pollfd) * (num_fds + 1));
    int *tmp = realloc(fds_to_module, sizeof(int) * (num_fds + 1));
    if (ptmp && tmp) {
        main_p = ptmp;
        fds_to_module = tmp;
        
        main_p[num_fds] = (struct pollfd) {
            .fd = fd,
            .events = POLLIN,
        };
        fds_to_module[num_fds] = self->idx;
        num_fds++;
        return 0;
    }
    return -1;
}

int init_modules(void) {
    int r = 0;
    for (int i = BUS; i < MODULES_NUM && !r; i++) {
        r = modules[i].init();
    }
    return r;
}

int loop_modules(void) {
    while (!quit) {
        int r = poll(main_p, num_fds, -1);
        if (r == -1 && errno != EINTR) {
            fprintf(stderr, "%s\n", strerror(errno));
            quit = LEAVE_W_ERR;
        }
        for (int i = 0; i < num_fds && !quit && r > 0; i++) {
            if (main_p[i].revents & POLLIN) {
                quit = poll_cb(i);
                r--;
            }
        }
    }
    return quit == LEAVE_W_ERR ? EXIT_FAILURE : EXIT_SUCCESS;
}

void destroy_modules(void) {
    for (int i = BUS; i < MODULES_NUM; i++) {
        modules[i].destroy();
    }
    
    for (int i = 0; i < num_fds; i++) {
        if (main_p[i].fd >= 0) {
            close(main_p[i].fd);
        }
    }
    
    free(fds_to_module);
    free(main_p);
    num_fds = 0;
}

int moduled_get_fd(module_t *self) {
    for (int i = 0; i < num_fds; i++) {
        if (fds_to_module[i] == self->idx) {
            return main_p[i].fd;
        }
    }
    return -1;
}

static int poll_cb(const int idx) {
    return modules[fds_to_module[idx]].poll_cb(main_p[idx].fd);
}
