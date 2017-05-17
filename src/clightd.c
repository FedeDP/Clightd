/* BEGIN_COMMON_COPYRIGHT_HEADER
 * 
 * clightd: C bus interface for linux to change screen brightness and capture frames from webcam device.
 * https://github.com/FedeDP/Clight/tree/master/clightd
 *
 * Copyright (C) 2017  Federico Di Pierro <nierro92@gmail.com>
 *
 * This file is part of clightd.
 * clightd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include <systemd/sd-bus.h>
#include <libudev.h>

#include "../inc/camera.h"
#include "../inc/gamma.h"
#include "../inc/commons.h"
#include "../inc/dpms.h"

static int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getactualbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
#ifdef GAMMA_PRESENT
static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
#endif
#ifndef DISABLE_FRAME_CAPTURES
static int method_captureframes(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
#endif
#ifdef DPMS_PRESENT
static int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_getdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
#endif
static void get_first_matching_device(struct udev_device **dev, const char *subsystem);
static void get_udev_device(const char *backlight_interface, const char *subsystem,
                            sd_bus_error **ret_error, struct udev_device **dev);
static int check_authorization(sd_bus_message *m);

static const char object_path[] = "/org/clightd/backlight";
static const char bus_interface[] = "org.clightd.backlight";
static struct udev *udev;
static sd_bus *bus = NULL;

/**
 * Bus spec: https://dbus.freedesktop.org/doc/dbus-specification.html
 */
static const sd_bus_vtable clightd_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("setbrightness", "si", NULL, method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getbrightness", "s", "i", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getmaxbrightness", "s", "i", method_getmaxbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getactualbrightness", "s", "i", method_getactualbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
#ifdef GAMMA_PRESENT
SD_BUS_METHOD("setgamma", "ssi", NULL, method_setgamma, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getgamma", "ss", "i", method_getgamma, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
#ifndef DISABLE_FRAME_CAPTURES
    SD_BUS_METHOD("captureframes", "si", "d", method_captureframes, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
#ifdef DPMS_PRESENT
    SD_BUS_METHOD("getdpms", "ss", "i", method_getdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("setdpms", "ssi", NULL, method_setdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getdpms_timeouts", "ss", "iii", method_getdpms_timeouts, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("setdpms_timeouts", "ssiii", NULL, method_setdpms_timeouts, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
    SD_BUS_VTABLE_END
};

/**
 * Brightness setter method
 */
static int method_setbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int value, r, max;
    struct udev_device *dev = NULL;
    const char *backlight_interface;
    
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }

    /* Read the parameters */
    r = sd_bus_message_read(m, "si", &backlight_interface, &value);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    /* Return an error if value is < 0 */
    if (value < 0) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Value must be greater or equal to 0.");
        return -EINVAL;
    }

    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }

    /**
     * Check if value is <= max_brightness value
     */
    max = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    if (value > max) {
        sd_bus_error_setf(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Value must be smaller than %d.", max);
        return -EINVAL;
    }

    char val[10];
    sprintf(val, "%d", value);
    r = udev_device_set_sysattr_value(dev, "brightness", val);
    if (r < 0) {
        udev_device_unref(dev);
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_ACCESS_DENIED, "Not authorized.");
        return r;
    }

    printf("New brightness value for %s: %d\n", udev_device_get_sysname(dev), value);

    udev_device_unref(dev);
    return sd_bus_reply_method_return(m, NULL);
}

/**
 * Current brightness getter method
 */
static int method_getbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;

    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }

    x = atoi(udev_device_get_sysattr_value(dev, "brightness"));
    printf("Current brightness value for %s: %d\n", udev_device_get_sysname(dev), x);

    udev_device_unref(dev);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

/**
 * Max brightness value getter method
 */
static int method_getmaxbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;

    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }

    x = atoi(udev_device_get_sysattr_value(dev, "max_brightness"));
    printf("Max brightness value for %s: %d\n", udev_device_get_sysname(dev), x);

    udev_device_unref(dev);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

/**
 * Actual brightness value getter method
 */
static int method_getactualbrightness(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int x, r;
    struct udev_device *dev = NULL;
    const char *backlight_interface;

    /* Read the parameters */
    r = sd_bus_message_read(m, "s", &backlight_interface);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    get_udev_device(backlight_interface, "backlight", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }

    x = atoi(udev_device_get_sysattr_value(dev, "actual_brightness"));
    printf("Actual brightness value for %s: %d\n", udev_device_get_sysname(dev), x);

    udev_device_unref(dev);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "i", x);
}

#ifdef GAMMA_PRESENT
static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
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
    
    if (temp < 1000 || temp > 10000) {
        error = EINVAL;
    } else {
        set_gamma(display, xauthority, temp, &error);
    }
    if (error) {
        if (error == EINVAL) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Temperature value should be between 1000 and 10000.");
        } else if (error == ENXIO) {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        }
        return -error;
    }

    printf("Gamma value set: %d\n", temp);
    return sd_bus_reply_method_return(m, NULL);
}

static int method_getgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int error = 0;
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int temp = get_gamma(display, xauthority, &error);
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
#endif

#ifndef DISABLE_FRAME_CAPTURES
/**
 * Frame capturing method
 */
static int method_captureframes(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r, error = 0, num_captures;
    struct udev_device *dev = NULL;
    const char *video_interface;

    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    r = sd_bus_message_read(m, "si", &video_interface, &num_captures);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (num_captures <= 0 || num_captures > 20) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Number of captures should be between 1 and 20.");
        return -EINVAL;
    }

    // if no video device is specified, try to get first matching device
    get_udev_device(video_interface, "video4linux", &ret_error, &dev);
    if (sd_bus_error_is_set(ret_error)) {
        return -sd_bus_error_get_errno(ret_error);
    }
    
    double val = capture_frames(udev_device_get_devnode(dev), num_captures, &error);
    if (error) {
        sd_bus_error_set_errno(ret_error, error);
        udev_device_unref(dev);
        return -error;
    }

    printf("%d frames captured by %s. Average brightness value: %lf\n", num_captures, udev_device_get_sysname(dev), val);
    udev_device_unref(dev);

    /* Reply with the response */
    return sd_bus_reply_method_return(m, "d", val);
}
#endif

#ifdef DPMS_PRESENT
static int method_getdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int dpms_state = get_dpms_state(display, xauthority);
    if (dpms_state == NO_X) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return NO_X;
    }
    
    if (dpms_state == DPMS_DISABLED) {
        printf("Dpms is currently disabled.\n");
    } else {
        printf("Current dpms state: %d\n", dpms_state);
    }
    return sd_bus_reply_method_return(m, "i", dpms_state);
}

static int method_setdpms(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    int level;
    
    /* Require polkit auth */
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ssi", &display, &xauthority, &level);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* 0 -> DPMSModeOn, 3 -> DPMSModeOff */
    if (level < 0 || level > 3) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong DPMS level value.");
        return -EINVAL;
    }
    
    int err = set_dpms_state(display, xauthority, level);
    if (err) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return err;
    }
    
    printf("New dpms state: %d\n", level);
    return sd_bus_reply_method_return(m, NULL);
}

static int method_getdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "ss", &display, &xauthority);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    struct dpms_timeout t = {0};
    int error = get_dpms_timeouts(display, xauthority, &t);
    if (error == NO_X) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return NO_X;
    }
    if (error == DPMS_DISABLED) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Dpms is disabled.");
        return DPMS_DISABLED;
    }
    
    printf("Current dpms timeouts:\tStandby: %ds\tSuspend: %ds\tOff:%ds.\n", t.standby, t.suspend, t.off);
    return sd_bus_reply_method_return(m, "iii", t.standby, t.suspend, t.off);
}

static int method_setdpms_timeouts(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *display = NULL, *xauthority = NULL;
    
    /* Require polkit auth */
    if (!check_authorization(m)) {
        sd_bus_error_set_errno(ret_error, EPERM);
        return -EPERM;
    }
    
    /* Read the parameters */
    int standby, suspend, off;
    int r = sd_bus_message_read(m, "ssiii", &display, &xauthority, &standby, &suspend, &off);
    if (r < 0) {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (standby < 0 || suspend < 0 || off < 0) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong DPMS timeouts values.");
        return -EINVAL;
    }
    
    struct dpms_timeout t = { .standby = standby, .suspend = suspend, .off = off };
    int err = set_dpms_timeouts(display, xauthority, &t);
    if (err) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not open X screen.");
        return err;
    }
    
    printf("New dpms timeouts:\tStandby: %ds\tSuspend: %ds\tOff:%ds.\n", t.standby, t.suspend, t.off);
    return sd_bus_reply_method_return(m, NULL);
}

#endif

/**
 * Set dev to first device in subsystem
 */
static void get_first_matching_device(struct udev_device **dev, const char *subsystem) {
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, subsystem);
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    if (devices) {
        const char *path;
        path = udev_list_entry_get_name(devices);
        *dev = udev_device_new_from_syspath(udev, path);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);
}

static void get_udev_device(const char *backlight_interface, const char *subsystem,
                            sd_bus_error **ret_error, struct udev_device **dev) {
    // if no backlight_interface is specified, try to get first matching device
    if (!backlight_interface || !strlen(backlight_interface)) {
        get_first_matching_device(dev, subsystem);
    } else {
        *dev = udev_device_new_from_subsystem_sysname(udev, subsystem, backlight_interface);
    }
    if (!(*dev)) {
        sd_bus_error_set_errno(*ret_error, ENODEV);
    }
}

static int check_authorization(sd_bus_message *m) {
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

int main(void) {
    sd_bus_slot *slot = NULL;
    int r;

    udev = udev_new();

    /* Connect to the system bus */
    r = sd_bus_default_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        goto finish;
    }

    /* Install the object */
    r = sd_bus_add_object_vtable(bus,
                                 &slot,
                                 object_path,
                                 bus_interface,
                                 clightd_vtable,
                                 NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
        goto finish;
    }

    /* Take a well-known service name so that clients can find us */
    r = sd_bus_request_name(bus, bus_interface, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        goto finish;
    }
    
    for (;;) {
        /* Process requests */
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
            goto finish;
        }

        /* we processed a request, try to process another one, right-away */
        if (r > 0) {
            continue;
        }

        /* Wait for the next request to process */
        r = sd_bus_wait(bus, (uint64_t) -1);
        if (r < 0) {
            fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
            goto finish;
        }
    }

finish:
    sd_bus_release_name(bus, bus_interface);
    if (slot) {
        sd_bus_slot_unref(slot);
    }
    if (bus) {
        sd_bus_flush_close_unref(bus);
    }
    udev_unref(udev);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
