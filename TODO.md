## 3.0
- [x] Als support
- [x] Use ALS_SUBSYSTEM and CAMERA_SUBSYSTEM where needed (put them in their headers)
- [x] Add a common interface for both camera and als (to be used by clight)
- [x] For Als, properly check that dev->name == "acpi-als" as iio subsystem covers much more sensors
- [ ] Move to gh wiki pages
- [x] Rename methods with camelcase
- [x] Fix for CaptureSensor method
- [ ] init_udev_monitor should be inited for ALS with acpi-als name filter (is it possible?)
- [x] Change directory structure -> sensors/{als.c,camera.c}, utils/{udev.c,polkit.c}, bus/{backlight,dpms,gamma,idle}. Rename clightd.c in main.c
- [x] CaptureWebcam should take a single frame and return a single double, adhering to CaptureSensor interface (ie: require a "s" and returning a "d")
- [x] Updated org.clightd.backlight policy
- [x] Drop setBrightness and getBrightness (useless methods) and rename setAll and getAll -> drop them from org.clightd.backlight policy too!
- [x] Document new API
- [x] IsXAvailable should take a "s" (interface) parameter

## 2.5 (3.1)
- [ ] Add gamma (and dpms + idle) support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
https://github.com/swaywm/wlroots/blob/master/examples/idle.c
https://github.com/minus7/redshift/tree/wayland

## 2.X (3.2+)

- [ ] Keep it up to date with possible ddcutil api changes
