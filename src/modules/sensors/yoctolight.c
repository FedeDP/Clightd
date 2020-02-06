#ifdef YOCTOLIGHT_PRESENT

#include <sensor.h>
#include <udev.h>
#include <libusb.h>

#define _dtor_                __attribute__((destructor (101))) // Used for libusb dtor

#define YOCTO_NAME            "YoctoLight"
#define YOCTO_ILL_MAX         500
#define YOCTO_ILL_MIN         22
#define YOCTO_INTERVAL        500 // ms
#define YOCTO_SUBSYSTEM       "usb"
#define YOCTO_PROPERTY        "idVendor"
#define YOCTO_VENDORID        "24e0"
#define YOCTO_PKT_SIZE        64
#define YOCTO_IFACE           0

enum usb_state { EMPTY, ATTACHED, CLAIMED };

typedef struct {
    libusb_device_handle *hdl;
    struct libusb_config_descriptor *config;
    uint8_t rdendp;
    enum usb_state st;
} ylight_state;

static bool get_dev_config(libusb_device *dev);
static void parse_settings(char *settings, int *min, int *max, int *interval);
static int init_usb_device(void);
static int destroy_usb_device(void);

static struct udev_monitor *mon;
static ylight_state state;

SENSOR(YOCTO_NAME);

static void _ctor_ init_libusb(void) {
    libusb_init(NULL);
}

static void _dtor_ destroy_libusb(void) {
    libusb_exit(NULL);
}

static bool get_dev_config(libusb_device *dev) {
    int ret = libusb_get_active_config_descriptor(dev, &state.config);
    if (ret == LIBUSB_ERROR_NOT_FOUND) {
        ret = libusb_get_config_descriptor(dev, 0, &state.config);
    }
    return ret == 0;
}

static bool validate_dev(void *dev) {
    const char *vendor_id = udev_device_get_sysattr_value(dev, YOCTO_PROPERTY);
    if (vendor_id && !strcmp(vendor_id, YOCTO_VENDORID)) {
        const char *product_id = udev_device_get_sysattr_value(dev, "idProduct");
        if (product_id) {
            int vendor = (int)strtol(vendor_id, NULL, 16); // hex
            int product = (int)strtol(product_id, NULL, 16); // hex
            state.hdl = libusb_open_device_with_vid_pid(NULL, vendor, product);
            if (state.hdl) {
                return get_dev_config(libusb_get_device(state.hdl));
            }
        }
        /* Always return true if action is "remove", ie: when called by udev monitor */
        const char *action = udev_device_get_action(dev);
        return action && !strcmp(action, UDEV_RM_ACTION);
    }
    return false;
}

static void fetch_dev(const char *interface, void **dev) {
    const udev_match match = { YOCTO_PROPERTY, YOCTO_VENDORID };
    get_udev_device(interface, YOCTO_SUBSYSTEM, &match, NULL, (struct udev_device **)dev);
}

static void fetch_props_dev(void *dev, const char **node, const char **action) {
    if (node) {
        *node =  udev_device_get_devnode(dev);
    }
    if (action) {
        *action = udev_device_get_action(dev);
    }
}

static void destroy_dev(void *dev) {
    udev_device_unref(dev);
    if (state.hdl) {
        libusb_close(state.hdl);
        state.hdl = NULL;
    }
    if (state.config) {
        libusb_free_config_descriptor(state.config);
        state.config = NULL;
    }
}

static int init_monitor(void) {
    return init_udev_monitor(YOCTO_SUBSYSTEM, &mon);
}

static void recv_monitor(void **dev) {
    *dev = udev_monitor_receive_device(mon);
}

static void destroy_monitor(void) {
    udev_monitor_unref(mon);
}

static void parse_settings(char *settings, int *min, int *max, int *interval) {
    const char opts[] = { 'i', 'm', 'M' };
    int *vals[] = { interval, min, max };
    
    /* Default values */
    *min = YOCTO_ILL_MIN;
    *max = YOCTO_ILL_MAX;
    *interval = YOCTO_INTERVAL;
    
    if (settings && strlen(settings)) {
        char *token; 
        char *rest = settings; 
        
        while ((token = strtok_r(rest, ",", &rest))) {
            char opt;
            int val;
            
            if (sscanf(token, "%c=%d", &opt, &val) == 2) {
                bool found = false;
                for (int i = 0; i < SIZE(opts) && !found; i++) {
                    if (opts[i] == opt) {
                        *(vals[i]) = val;
                        found = true;
                    }
                }
                
                if (!found) {
                    fprintf(stderr, "Option %c not found.\n", opt);
                }
            } else {
                fprintf(stderr, "Expected a=b format.\n");
            }
        }
    }
    
    /* Sanity checks */
    if (*interval < 0 || *interval > 1000) {
        fprintf(stderr, "Wrong interval value. Resetting default.\n");
        *interval = YOCTO_INTERVAL;
    }
    if (*min < 0) {
        fprintf(stderr, "Wrong min value. Resetting default.\n");
        *min = YOCTO_ILL_MIN;
    }
    if (*max < 0) {
        fprintf(stderr, "Wrong max value. Resetting default.\n");
        *max = YOCTO_ILL_MAX;
    }
    if (*min > *max) {
        fprintf(stderr, "Wrong min/max values. Resetting defaults.\n");
        *min = YOCTO_ILL_MIN;
        *max = YOCTO_ILL_MAX;
    }
}

static int init_usb_device(void) {
    int ret = libusb_kernel_driver_active(state.hdl, YOCTO_IFACE);
    if (ret < 0) {
        fprintf(stderr, "libusb_kernel_driver_active\n");
        return -1;
    } 
    if (ret > 0) {
        printf("Need to detach kernel driver\n");
        ret = libusb_detach_kernel_driver(state.hdl, YOCTO_IFACE);
        if (ret != 0) {
            fprintf(stderr, "libusb_detach_kernel_driver\n");
            return -1;
        }
    }
    state.st |= ATTACHED;
    
    ret = libusb_claim_interface(state.hdl, YOCTO_IFACE);
    if (ret < 0) {
        fprintf(stderr, "libusb_claim_interface\n");
        return -1;
    }
    state.st |= CLAIMED;
    
    const struct libusb_interface_descriptor *ifd = &state.config->interface[YOCTO_IFACE].altsetting[0];
    for (int j = 0; j < ifd->bNumEndpoints; j++) {
        if ((ifd->endpoint[j].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            state.rdendp = ifd->endpoint[j].bEndpointAddress;
        }
    }
    return -(state.rdendp == 0);
}

static int destroy_usb_device(void) {
    int ret = 0;
    for (int i = CLAIMED; i > EMPTY; i--) {
        if (state.st & i) {
            switch (i) {
                case CLAIMED:
                    ret = libusb_release_interface(state.hdl, YOCTO_IFACE);
                    if (ret && ret != LIBUSB_ERROR_NOT_FOUND && ret != LIBUSB_ERROR_NO_DEVICE) {
                        fprintf(stderr, "Failed: libusb_release_interface.\n");
                    }
                    break;
                case ATTACHED:
                    ret = libusb_attach_kernel_driver(state.hdl, YOCTO_IFACE);
                    if(ret < 0 && ret != LIBUSB_ERROR_NO_DEVICE) {
                        fprintf(stderr, "Failed: libusb_attach_kernel_driver.\n");
                    }
                    break;
                default:
                    break;
            }
        }
    }
    state.rdendp = 0;
    state.st = EMPTY;
    return ret;
}

static int capture(void *dev, double *pct, const int num_captures, char *settings) {
    int min, max, interval;
    parse_settings(settings, &min, &max, &interval);
    int ret = -num_captures;
    
    if (state.hdl) {
        if (init_usb_device() == 0) {
            for (int i = 0; i < num_captures; i++) {
                uint8_t buf[YOCTO_PKT_SIZE] = { 0 };
                int trans = 0;
                int rret = (libusb_interrupt_transfer(state.hdl, state.rdendp, 
                                                      buf, YOCTO_PKT_SIZE, &trans, interval));
                if (rret == 0
                    && trans == YOCTO_PKT_SIZE) {
                    
                    pct[i] = atof((const char *)&buf[3]);
                    printf("ambient: %.2lf\n", pct[i]);
                    ret++;
                }
                
                /* Returns timeout if old value == new value! */
                if (rret == LIBUSB_ERROR_TIMEOUT && i > 0 && pct[i - 1]) {
                    pct[i] = pct[i - 1];
                    ret++;
                }
                printf("toppp %d -> %d\n", rret, trans);
            }
            destroy_usb_device();
        }
    }
    return ret; // 0 if all requested captures are fullfilled
}

#endif
