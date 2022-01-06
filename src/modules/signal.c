#include <commons.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <signal.h>

MODULE("SIGNAL");

static sigset_t mask;


static void block_signals(void) {
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

/*
 * Block signals for any thread spawned,
 * so that only main thread will indeed receive SIGTERM/SIGINT signals
 *
 * It is needed as some libraries (libusb, libddcutil, libpipewire) spawn threads
 * that would mess with the signal receiving mechanism of Clightd
 * 
 * NOTE: we cannot use a normal constructor because libraries ctor are
 * always run before program ctor, as expected;
 * see: https://github.com/lattera/glibc/blob/895ef79e04a953cac1493863bcae29ad85657ee1/elf/dl-init.c#L109
 */
__attribute__((section(".preinit_array"), used)) static typeof(block_signals) *preinit_p = block_signals;

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
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
        modules_quit(0);
    }
}

static void destroy(void) {

}
