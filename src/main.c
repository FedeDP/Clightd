/* BEGIN_COMMON_COPYRIGHT_HEADER
 * 
 * clightd: C bus interface for linux to change screen brightness and capture frames from webcam device.
 * https://github.com/FedeDP/Clight/tree/master/clightd
 *
 * Copyright (C) 2018  Federico Di Pierro <nierro92@gmail.com>
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

#include <modules.h>

static const char bus_interface[] = "org.clightd.clightd";

int main(void) {
    udev = udev_new();
    int r = init_modules();
    if (!r) {
        r = sd_bus_request_name(bus, bus_interface, 0);
        if (r < 0) {
            fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        } else {
            /* Drop root privileges */
            drop_priv();
            r = loop_modules();
            sd_bus_release_name(bus, bus_interface);
        }
    } else {
        fprintf(stderr, "Failed to init all modules.\n");
    }
    destroy_modules();
    udev_unref(udev);
    return r;
}
