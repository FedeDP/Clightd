#include "commons.h"

int bus_sender_fill_creds(sd_bus_message *m);
const char *bus_sender_runtime_dir(void);
const char *bus_sender_xauth(void);

void make_valid_obj_path(char *storage, size_t size, const char *root, const char *basename);
