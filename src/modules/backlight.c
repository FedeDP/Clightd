#include <commons.h>
#include <module/map.h>
#include <polkit.h>
#include <udev.h>

static inline void *fetch_bl(sd_bus_message *m);

/* Exposed */
static int method_setallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_raiseallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_lowerallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setbrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getbrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_raisebrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_lowerbrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clightd/clightd/Backlight";
static const char bus_interface[] = "org.clightd.clightd.Backlight";
static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetAll", "d(bdu)s", "b", method_setallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetAll", "s", "a(sd)", method_getallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("RaiseAll", "d(bdu)s", "b", method_raiseallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("LowerAll", "d(bdu)s", "b", method_lowerallbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Set", "d(bdu)s", "b", method_setbrightness1, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", "s", "(sd)", method_getbrightness1, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Raise", "d(bdu)s", "b", method_raisebrightness1, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Lower", "d(bdu)s", "b", method_lowerbrightness1, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Changed", "sd", 0),
    SD_BUS_VTABLE_END
};

extern int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
extern int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
extern int method_raisebrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
extern int method_lowerbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
extern map_ret_code get_backlight(void *userdata, const char *key, void *data);
extern map_t *bls;

MODULE("BACKLIGHT");

static void module_pre_start(void) {

}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void init(void) {
    int r = sd_bus_add_object_vtable(bus,
                                 NULL,
                                 object_path,
                                 bus_interface,
                                 vtable,
                                 NULL);
    if (r < 0) {
        m_log("Failed to issue method call: %s\n", strerror(-r));
    }
}

static void receive(const msg_t *msg, const void *userdata) {

}

static void destroy(void) {
    
}

static int method_setallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return method_setbrightness(m, NULL, ret_error);
}

static int method_getallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return method_getbrightness(m, NULL, ret_error);
}

static int method_raiseallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return method_raisebrightness(m, NULL, ret_error);
}

static int method_lowerallbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return method_lowerbrightness(m, NULL, ret_error);
}

static inline void *fetch_bl(sd_bus_message *m) {
    void *bl = NULL;
    const char *sn = NULL;
    // Do not check for errors: method_getbrightness1 has only a "s" signature!
    sd_bus_message_skip(m, "d(bdu)");
    int r = sd_bus_message_read(m, "s", &sn);
    if (r >= 0) {
        bl = map_get(bls, sn);
        sd_bus_message_rewind(m, true);
    }
    return bl;
}

static int method_setbrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    void *bl = fetch_bl(m);
    if (!bl) {
        return -ENOENT;
    }
    return method_setbrightness(m, bl, ret_error);
}

static int method_getbrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    void *bl = fetch_bl(m);
    if (!bl) {
        return -ENOENT;
    }
    const char *sn = NULL;
    sd_bus_message_read(m, "s", &sn);
    double pct = 0.0;
    get_backlight(&pct, NULL, bl);
    return sd_bus_reply_method_return(m, "(sd)", sn, pct);
}

static int method_raisebrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    void *bl = fetch_bl(m);
    if (!bl) {
        return -ENOENT;
    }
    return method_raisebrightness(m, bl, ret_error);
}

static int method_lowerbrightness1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    void *bl = fetch_bl(m);
    if (!bl) {
        return -ENOENT;
    }
    return method_lowerbrightness(m, bl, ret_error);
}
