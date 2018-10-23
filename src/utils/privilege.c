#include <unistd.h>

static const uid_t unprivileged = 1000;
static uid_t privileged;

int drop_priv(void) {
    privileged = geteuid();
    return setresuid(-1, unprivileged, privileged);
}

int gain_priv(void) {
    if (geteuid() == unprivileged) {
        return setresuid(-1, privileged, -1);
    }
    return 0;
}
