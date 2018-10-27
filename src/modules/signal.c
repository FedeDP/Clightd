#include <modules.h>
#include <sys/signalfd.h>
#include <signal.h>

MODULE(SIGNAL);

static int init(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    return REGISTER_FD(signalfd(-1, &mask, 0));
}

static int callback(const int fd) {
    struct signalfd_siginfo fdsi;
    ssize_t s = read(fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        MODULE_ERR("An error occurred while getting signalfd data.\n");
    }
    MODULE_INFO("Received signal %d. Leaving.\n", fdsi.ssi_signo);
    return SIGNAL_RCV;
}

static void destroy(void) {

}
