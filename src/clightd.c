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

#include "../inc/camera.h"
#include "../inc/gamma.h"
#include "../inc/dpms.h"
#include "../inc/backlight.h"
#include "../inc/idle.h"
#include "../inc/udev.h"

static const char object_path[] = "/org/clightd/backlight";
static const char bus_interface[] = "org.clightd.backlight";

/**
 * Bus spec: https://dbus.freedesktop.org/doc/dbus-specification.html
 */
static const sd_bus_vtable clightd_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("setbrightness", "si", "i", method_setbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getbrightness", "s", "i", method_getbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getmaxbrightness", "s", "i", method_getmaxbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getactualbrightness", "s", "i", method_getactualbrightness, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("isbacklightinterfaceenabled", "s", "i", method_isinterface_enabled, SD_BUS_VTABLE_UNPRIVILEGED),
#ifdef GAMMA_PRESENT
    SD_BUS_METHOD("setgamma", "ssi", "i", method_setgamma, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getgamma", "ss", "i", method_getgamma, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
#ifndef DISABLE_FRAME_CAPTURES
    SD_BUS_METHOD("captureframes", "si", "ad", method_captureframes, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
#ifdef DPMS_PRESENT
    SD_BUS_METHOD("getdpms", "ss", "i", method_getdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("setdpms", "ssi", "i", method_setdpms, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("getdpms_timeouts", "ss", "iii", method_getdpms_timeouts, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("setdpms_timeouts", "ssiii", "iii", method_setdpms_timeouts, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
#ifdef IDLE_PRESENT
    SD_BUS_METHOD("getidletime", "ss", "i", method_get_idle_time, SD_BUS_VTABLE_UNPRIVILEGED),
#endif
    SD_BUS_VTABLE_END
};

int main(void) {
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
                                 NULL,
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
    if (bus) {
        sd_bus_flush_close_unref(bus);
    }
    udev_unref(udev);
    
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
