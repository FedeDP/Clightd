#include "../inc/polkit.h"

int check_authorization(sd_bus_message *m) {
    int authorized = 0;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus_creds *c = sd_bus_message_get_creds(m);
    
    const char *busname;
    int r = sd_bus_creds_get_unique_name(c, &busname);
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
        goto end;
    }
    
    char action_id[100] = {0};
    snprintf(action_id, sizeof(action_id), "%s.%s", sd_bus_message_get_destination(m), sd_bus_message_get_member(m));
    
    r = sd_bus_call_method(bus, "org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
                           "org.freedesktop.PolicyKit1.Authority", "CheckAuthorization", &error, &reply,
                           "(sa{sv})sa{ss}us", "system-bus-name", 1, "name", "s", busname, action_id, NULL, 0, "");
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
    } else {
        /* only read first boolean -> complete signature is "bba{ss}" but we only need first (authorized boolean) */
        r = sd_bus_message_read(reply, "(bba{ss})", &authorized, NULL, NULL);
        if (r < 0) {
            fprintf(stderr, "%s\n", strerror(-r));
        }
    }
    
    end:
    if (reply) {
        sd_bus_message_unref(reply);
    }
    if (error.message) {
        sd_bus_error_free(&error);
    }
    return authorized;
}
