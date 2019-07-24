/* BEGIN_COMMON_COPYRIGHT_HEADER
 * 
 * clightd: C bus interface for linux to change screen brightness and capture frames from webcam device.
 * https://github.com/FedeDP/Clight/tree/master/clightd
 *
 * Copyright (C) 2019  Federico Di Pierro <nierro92@gmail.com>
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

#include <commons.h>

static const char bus_interface[] = "org.clightd.clightd";

/* Every module needs these; let's init them before any module */
void modules_pre_start(void) {
    udev = udev_new();
} 

static void check_opts(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            printf("Clightd: dbus API to easily set screen backlight, gamma temperature and get ambient brightness through webcam frames capture or ALS devices.\n");
            printf("* Current version: %s\n", VERSION);
            printf("* https://github.com/FedeDP/Clightd\n");
            printf("* Copyright (C) 2019  Federico Di Pierro <nierro92@gmail.com>\n");
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char *argv[]) {
    check_opts(argc, argv);
    int r = sd_bus_request_name(bus, bus_interface, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
    } else {
        r = modules_loop();
        sd_bus_release_name(bus, bus_interface);
    }
    udev_unref(udev);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
