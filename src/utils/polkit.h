#include <commons.h>

#define ASSERT_AUTH() \
    if (!check_authorization(m)) { \
        sd_bus_error_set_errno(ret_error, EPERM); \
        return -EPERM; \
    }

int check_authorization(sd_bus_message *m);
