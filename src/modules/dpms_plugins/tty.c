#ifdef DPMS_PRESENT

#include <commons.h>
#include <linux/tiocl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <fcntl.h>

static int is_a_console(int fd);
static int open_a_console(const char *fnam);
static int getfd(void);

static const char *conspath[] = {
    "/proc/self/fd/0",
    "/dev/tty",
    "/dev/tty0",
    "/dev/vc/0",
    "/dev/systty",
    "/dev/console",
    NULL
};

static int cons_fd;

int tty_get_dpms_state(void) {
    int ret = -1;
    cons_fd = getfd();
    if (cons_fd >= 0) {
        char c = TIOCL_BLANKEDSCREEN;
        ret = ioctl(cons_fd, TIOCLINUX, &c);
        if (ret < 0) {
            fprintf(stderr, "TIOCL_BLANKEDSCREEN failed: %s\n", strerror(errno));
        }
    }
    return ret;
}

int tty_set_dpms_state(int dpms_level) {
    int ret = -1;
    cons_fd = getfd();
    if (cons_fd >= 0) {        
        char c;
        switch (dpms_level) {
            case 0:
                c = TIOCL_UNBLANKSCREEN;
                break;
            default:
                c = TIOCL_BLANKSCREEN;
                break;
        }
        ret = ioctl(cons_fd, TIOCLINUX, &c);
        if (ret) {
            fprintf(stderr, "TIOCL %d failed: %s\n", c, strerror(errno));
        }
    }
    return ret;
}

void tty_close(void) {
    close(cons_fd);
}

static int is_a_console(int fd) {
    char arg = 0;
    return (isatty(fd) && ioctl(fd, KDGKBTYPE, &arg) == 0 && ((arg == KB_101) || (arg == KB_84)));
}

static int open_a_console(const char *fnam) {
    int fd = open(fnam, O_RDWR);
    if (fd < 0) {
        fd = open(fnam, O_WRONLY);
    }
    if (fd < 0) {
        fd = open(fnam, O_RDONLY);
    }
    return fd;
}

static int getfd(void) {
    /* If fd is already initialized, use that! */
    if (cons_fd != 0) {
        return cons_fd;
    }
    
    int fd;
    for (int i = 0; conspath[i]; i++) {
        if ((fd = open_a_console(conspath[i])) >= 0) {
            if (is_a_console(fd)) {
                return fd;
            }
            close(fd);
        }
    }
    
    for (fd = 0; fd < 3; fd++) {
        if (is_a_console(fd)) {
            return fd;
        }
    }
    
    fprintf(stderr,"Couldn't get a file descriptor referring to the console.\n");
    
    /* total failure */
    exit(EXIT_FAILURE);
}

#endif
