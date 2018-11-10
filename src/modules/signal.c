#include <module/module_easy.h>
#include <commons.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <signal.h>

MODULE("SIGNAL");

static void module_pre_start(void) {
    
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    m_register_fd(signalfd(-1, &mask, 0), true, NULL);
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        struct signalfd_siginfo fdsi;
        ssize_t s = read(msg->fd_msg->fd, &fdsi, sizeof(struct signalfd_siginfo));
        if (s != sizeof(struct signalfd_siginfo)) {
            m_log("An error occurred while getting signalfd data.\n");
        }
        m_log("Received signal %d. Leaving.\n", fdsi.ssi_signo);
        modules_quit(SIGNAL_RCV);
    }
}

static void destroy(void) {

}
