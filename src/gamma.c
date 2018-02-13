/**
 * Thanks to http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/ 
 * and to improvements made here: http://www.zombieprototypes.com/?p=210.
 **/

#ifdef GAMMA_PRESENT

#include "../inc/gamma.h"
#include "../inc/polkit.h"
#include <X11/extensions/Xrandr.h>
#include <math.h>

static unsigned short clamp(double x, double max);
static unsigned short get_red(int temp);
static unsigned short get_green(int temp);
static unsigned short get_blue(int temp);
static int get_temp(const unsigned short R, const unsigned short B);
static int set_gamma(int temp, Display *dpy);
static int get_gamma(Display *dpy);

typedef struct {
    int fd;
    unsigned int target_temp;
    unsigned int smooth_step;
    unsigned int smooth_wait;
    unsigned int current_temp;
    Display *dpy;
} smooth_change;

static smooth_change sc;

void set_gamma_smooth_fd(int fd) {
    sc.fd = fd;
}

int gamma_smooth_cb(void) {
    uint64_t t;
    // nonblocking mode!
    read(sc.fd, &t, sizeof(uint64_t));
    
    if (sc.target_temp < sc.current_temp) {
        sc.current_temp = sc.current_temp - sc.smooth_step < sc.target_temp ? 
                        sc.target_temp :
                        sc.current_temp - sc.smooth_step;
    } else {
        sc.current_temp = sc.current_temp + sc.smooth_step > sc.target_temp ? 
        sc.target_temp :
        sc.current_temp + sc.smooth_step;
    }
    if (set_gamma(sc.current_temp, sc.dpy) == sc.target_temp) {
        XCloseDisplay(sc.dpy);
        unsetenv("XAUTHORITY");
    } else {
        struct itimerspec timerValue = {{0}};
        timerValue.it_value.tv_sec = 0;
        timerValue.it_value.tv_nsec = 1000 * 1000 * sc.smooth_wait; // ms
        return timerfd_settime(sc.fd, 0, &timerValue, NULL);
    }
    return 0;
}

int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int temp, error = 0;
    const char *display = NULL, *xauthority = NULL;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi", &display, &xauthority, &temp);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    const int is_smooth;
    const unsigned int smooth_step, smooth_wait;
    
    sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, "buu");
    sd_bus_message_read(m, "buu", &is_smooth, &smooth_step, &smooth_wait);
    sd_bus_message_exit_container(m);
    
    if (temp < 1000 || temp > 10000) {
        error = EINVAL;
    } else {
        /* set xauthority cookie */
        setenv("XAUTHORITY", xauthority, 1);
        
        Display *dpy = XOpenDisplay(display);
        if (dpy == NULL) {
            perror("XopenDisplay");
            error = ENXIO;
            /* Drop xauthority cookie */
            unsetenv("XAUTHORITY");
        } else {
            if (is_smooth && smooth_step && smooth_wait) {
                sc.target_temp = temp;
                sc.smooth_step = smooth_step;
                sc.smooth_wait = smooth_wait;
                sc.dpy = dpy;
                sc.current_temp = get_gamma(sc.dpy);
                
                printf("Gamma value target set (smooth): %d\n", temp);
                
                gamma_smooth_cb(); // xauthority cookie will be dropped in smooth cb
            } else {
                set_gamma(temp, dpy);
                printf("Gamma value set: %d\n", temp);
            
                XCloseDisplay(dpy);
                /* Drop xauthority cookie */
                unsetenv("XAUTHORITY");
            }
        }
    }
    if (error) {
        if (error == EINVAL) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Temperature value should be between 1000 and 10000.");
        } else if (error == ENXIO) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        }
        return -error;
    }
    
    return sd_bus_reply_method_return(m, "b", !error);
}

int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int error = 0, temp;
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* set xauthority cookie */
    setenv("XAUTHORITY", xauthority, 1);
    
    Display *dpy = XOpenDisplay(display);
    if (dpy == NULL) {
        perror("XopenDisplay");
        error = ENXIO;
    } else {
        temp = get_gamma(dpy);
        XCloseDisplay(dpy);
    }
    
    /* Drop xauthority cookie */
    unsetenv("XAUTHORITY");
    
    if (error) {
        if (error == ENXIO) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        } else {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to get screen temperature.");
        }
        return -error;
    }
    
    printf("Current gamma value: %d\n", temp);
    return sd_bus_reply_method_return(m, "i", temp);
}

static unsigned short clamp(double x, double max) {
    if (x > max) {
        return max;
    }
    return x;
}

static unsigned short get_red(int temp) {
    if (temp <= 6500) {
        return 255;
    }
    const double a = 351.97690566805693;
    const double b = 0.114206453784165;
    const double c = -40.25366309332127;
    const double new_temp = ((double)temp / 100) - 55;
    
    return clamp(a + b * new_temp + c * log(new_temp), 255);
}

static unsigned short get_green(int temp) {
    double a, b, c;
    double new_temp;
    if (temp <= 6500) {
        a = -155.25485562709179;
        b = -0.44596950469579133;
        c = 104.49216199393888;
        new_temp = ((double)temp / 100) - 2;
    } else {
        a = 325.4494125711974;
        b = 0.07943456536662342;
        c = -28.0852963507957;
        new_temp = ((double)temp / 100) - 50;        
    }
    return clamp(a + b * new_temp + c * log(new_temp), 255);
}

static unsigned short get_blue(int temp) {
    if (temp <= 1900) {
        return 0;
    }
    
    if (temp < 6500) {
        const double new_temp = ((double)temp / 100) - 10;
        const double a = -254.76935184120902;
        const double b = 0.8274096064007395;
        const double c = 115.67994401066147;
        
        return clamp(a + b * new_temp + c * log(new_temp), 255);
    }
    return 255;
}

/* Thanks to: https://github.com/neilbartlett/color-temperature/blob/master/index.js */
static int get_temp(const unsigned short R, const unsigned short B) {
    int temperature;
    int min_temp = B == 255 ? 6500 : 1000; // lower bound
    int max_temp = R == 255 ? 6500 : 10000; // upper bound
    unsigned short testR, testB;
    
    /* Compute first temperature with same R and B value as parameters */
    do {
        temperature = (max_temp + min_temp) / 2;
        testR = get_red(temperature);
        testB = get_blue(temperature);
        if ((double) testB / testR > (double) B / R) {
            max_temp = temperature;
        } else {
            min_temp = temperature;
        }
    } while (testR != R || testB != B);
    
    /* try to fit value in 50-steps temp -> ie: instead of 5238, try 5200 or 5250 */
    if (temperature % 50 != 0) {
        int tmp_temp = temperature - temperature % 50;
        if (get_red(tmp_temp) == R && get_blue(tmp_temp) == B) {
            temperature = tmp_temp;
        } else {
            tmp_temp = temperature + 50 - temperature % 50;
            if (get_red(tmp_temp) == R && get_blue(tmp_temp) == B) {
                temperature = tmp_temp;
            }
        }
    }
    
    return temperature;
}

static int set_gamma(int temp, Display *dpy) {
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    
    double red = get_red(temp) / (double)255;
    double green = get_green(temp) / (double)255;
    double blue = get_blue(temp) / (double)255;
        
    for (int i = 0; i < res->ncrtc; i++) {
        int crtcxid = res->crtcs[i];
        int size = XRRGetCrtcGammaSize(dpy, crtcxid);
        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);
        for (int j = 0; j < size; j++) {
            double g = 65535.0 * j / size;
            crtc_gamma->red[j] = g * red;
            crtc_gamma->green[j] = g * green;
            crtc_gamma->blue[j] = g * blue;
        }
        XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
        XFree(crtc_gamma);
    }
    XRRFreeScreenResources(res);
    return temp;
}

static int get_gamma(Display *dpy) {
    int temp = -1;
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (res->ncrtc > 0) {
        XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(dpy, res->crtcs[0]);
        temp = get_temp(clamp(crtc_gamma->red[1], 255), clamp(crtc_gamma->blue[1], 255));
        XFree(crtc_gamma);
    }
    XRRFreeScreenResources(res);
    return temp;
}

#endif
