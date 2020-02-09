#ifdef YOCTOLIGHT_PRESENT

#include <sensor.h>
#include <udev.h>
#include <libusb.h>

#define YOCTO_ERR(fmt, ...)  fprintf(stderr, fmt, ##__VA_ARGS__); return -1;

#define _dtor_                __attribute__((destructor (101))) // Used for libusb dtor

#define YOCTO_NAME            "YoctoLight"
#define YOCTO_ILL_MAX         500
#define YOCTO_ILL_MIN         22
#define YOCTO_INTERVAL        500 // ms
#define YOCTO_SUBSYSTEM       "usb"
#define YOCTO_PROPERTY        "idVendor"
#define YOCTO_VENDORID        "24e0"
#define YOCTO_PKT_SIZE        64
#define YOCTO_SERIAL_LEN      20
#define YOCTO_IFACE           0

#define YOCTO_CONF_RESET      0
#define YOCTO_CONF_START      1

#define YOCTO_PKT_STREAM         0
#define YOCTO_PKT_CONF           1

#define YOCTO_STREAM_NOTICE         3
#define YOCTO_STREAM_NOTICE_V2      7

#define YOCTO_NOTIFY_PKT_STREAMREADY 6
#define YOCTO_NOTIFY_PKT_STREAMREADY_2 49 // sometimes 49 is reported as streamready value too!

#define TO_SAFE_U16(safe,unsafe)        {(safe).low = (unsafe)&0xff; (safe).high=(unsafe)>>8;}
#define FROM_SAFE_U16(safe,unsafe)      {(unsafe) = (safe).low |((uint16_t)((safe).high)<<8);}

enum usb_state { EMPTY, ATTACHED, CLAIMED };

typedef struct {
    uint8_t low;
    uint8_t high;
} SAFE_U16;

typedef struct{
    char serial[YOCTO_SERIAL_LEN];
    uint8_t type;
} Notification_header;

typedef union {
    uint8_t firstByte;
    Notification_header head;
} USB_Notify_Pkt;

#ifndef CPU_BIG_ENDIAN
typedef struct {
    uint8_t pktno    : 3;
    uint8_t stream   : 5;
    uint8_t pkt      : 2;
    uint8_t size     : 6;
} YSTREAM_Head;
#else
typedef struct {
    uint8_t stream   : 5;
    uint8_t pktno    : 3;
    uint8_t size     : 6;
    uint8_t pkt      : 2;
} YSTREAM_Head;
#endif

typedef union{
    struct{
        SAFE_U16 api;
        uint8_t ok;
        uint8_t ifaceno;
        uint8_t nbifaces;
    } reset;
    struct{
        uint8_t nbifaces;
        uint8_t ack_delay;
    } start;
} USB_Conf_Pkt;

typedef union {
    uint8_t data[YOCTO_PKT_SIZE];
    struct {
        YSTREAM_Head head;
        USB_Conf_Pkt conf;
    } confpkt;
} USB_Packet;

typedef struct {
    libusb_device_handle *hdl;
    struct libusb_config_descriptor *config;
    uint8_t rdendp;
    uint8_t wrendp;
    int interval;
    enum usb_state st;
} ylight_state;

static bool get_dev_config(libusb_device *dev);
static void parse_settings(char *settings, int *min, int *max, int *interval);
static int init_usb_device(void);
static int start_usb_device(USB_Packet *rpkt);
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

static inline void build_conf_packet(USB_Packet *pkt, int stream_type) {
    memset(pkt, 0, sizeof(USB_Packet));
    pkt->confpkt.head.pkt    = YOCTO_PKT_CONF;
    pkt->confpkt.head.stream = stream_type;
    pkt->confpkt.head.size   = YOCTO_PKT_SIZE - sizeof(pkt->confpkt.head);
    pkt->confpkt.head.pktno  = 0;
}

static inline int send_and_recv_packet(USB_Packet *pkt, USB_Packet *rpkt) {
    int pkt_type = pkt->confpkt.head.pkt;
    int stream_type = pkt->confpkt.head.stream;
    
    int trans = 0;
    int ret = libusb_interrupt_transfer(state.hdl, state.wrendp, (unsigned char *)pkt, YOCTO_PKT_SIZE, &trans, state.interval);
    if (ret == 0 && trans == YOCTO_PKT_SIZE) {
        trans = 0;
        memset(rpkt, 0, sizeof(USB_Packet));
        while (1) {
            ret = libusb_interrupt_transfer(state.hdl, state.rdendp, (unsigned char *)rpkt, YOCTO_PKT_SIZE, &trans, state.interval);
            if (rpkt->confpkt.head.pkt == pkt_type && rpkt->confpkt.head.stream == stream_type) {
                return 0;
            }
        }
    }
    return -1;
}

static int init_usb_device(void) {
    int ret = libusb_kernel_driver_active(state.hdl, YOCTO_IFACE);
    if (ret < 0) {
        YOCTO_ERR("libusb_kernel_driver_active\n");
    } 
    if (ret > 0) {
        printf("Need to detach kernel driver\n");
        ret = libusb_detach_kernel_driver(state.hdl, YOCTO_IFACE);
        if (ret != 0) {
            YOCTO_ERR("libusb_detach_kernel_driver\n");
        }
    }
    state.st |= ATTACHED;
    
    ret = libusb_claim_interface(state.hdl, YOCTO_IFACE);
    if (ret < 0) {
        YOCTO_ERR("libusb_claim_interface\n");
    }
    state.st |= CLAIMED;
    
    const struct libusb_interface_descriptor *ifd = &state.config->interface[YOCTO_IFACE].altsetting[0];
    for (int j = 0; j < ifd->bNumEndpoints; j++) {
        if ((ifd->endpoint[j].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            state.rdendp = ifd->endpoint[j].bEndpointAddress;
        } else {
            state.wrendp = ifd->endpoint[j].bEndpointAddress;;
        }
    }
    return -(state.rdendp == 0 || state.wrendp == 0);
}

static int start_usb_device(USB_Packet *rpkt) {
    /** Send reset **/
    USB_Packet pkt;
    build_conf_packet(&pkt, YOCTO_CONF_RESET);
    pkt.confpkt.conf.reset.ok = 1;
    TO_SAFE_U16(pkt.confpkt.conf.reset.api, 0x0209);
    
    if (send_and_recv_packet(&pkt, rpkt) == -1) {
        YOCTO_ERR("Error sending packet: RESET.\n");
    }
    /** **/
    
    /** Send Start **/
    build_conf_packet(&pkt, YOCTO_CONF_START);
    pkt.confpkt.conf.start.nbifaces = 1;
    pkt.confpkt.conf.start.ack_delay = 0;
    if (send_and_recv_packet(&pkt, rpkt) == -1) {
        YOCTO_ERR("Error sending packet: START.\n");
    }
    /** **/
    
    /** Check **/
    int nextiface = rpkt->confpkt.conf.start.nbifaces;
    if (nextiface != 0) {
        YOCTO_ERR("Device has not been started correctly\n");
    }
    /** **/
    
    /** Wait on stream ready **/
    int trans = 0;
    memset(rpkt, 0, sizeof(USB_Packet));
    while (1) {
        libusb_interrupt_transfer(state.hdl, state.rdendp, (unsigned char *) rpkt, YOCTO_PKT_SIZE, &trans, state.interval);
        if (rpkt->confpkt.head.pkt == YOCTO_PKT_STREAM) {
            if (rpkt->confpkt.head.stream == YOCTO_STREAM_NOTICE || rpkt->confpkt.head.stream == YOCTO_STREAM_NOTICE_V2) {
                uint8_t *data =((uint8_t*)&rpkt->confpkt.head) + sizeof(YSTREAM_Head);
                USB_Notify_Pkt *not = (USB_Notify_Pkt*)data;
                if (not->firstByte == 1) {
                    if (not->head.type == YOCTO_NOTIFY_PKT_STREAMREADY || not->head.type == YOCTO_NOTIFY_PKT_STREAMREADY_2) {
                        /* Device is now ready. */
                        break;
                    }
                }
            }
        }
    }
    /** **/
    
    return 0;
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
    int ret = -ENODEV;
    
    if (state.hdl) {
        state.interval = interval;
        USB_Packet rpkt = { 0 };
        if (init_usb_device() == 0 && start_usb_device(&rpkt) == 0) {
            ret = 0;
            for (int i = 0; i < num_captures; i++) {
                int trans = 0;
                libusb_interrupt_transfer(state.hdl, state.rdendp, (unsigned char *) &rpkt, YOCTO_PKT_SIZE, &trans, interval);
                double illuminance = atof((char *)&rpkt.data[3]);
                if (illuminance > max) {
                    illuminance = max;
                } else if (illuminance < min) {
                    illuminance = min;
                }
                pct[i] = illuminance / max;
            }
        }
        destroy_usb_device();
    }
    return ret; // 0 if all requested captures are fullfilled
}

#endif
