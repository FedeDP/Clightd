## 2.5 (3.0)
- [ ] Als support
- [ ] Use ALS_SUBSYSTEM and CAMERA_SUBSYSTEM where needed (put them in their headers)
- [ ] Add a common interface for both camera and als (to be used by clight)
- [ ] For Als, properly check that dev->name == "acpi-als" as iio subsystem covers much more sensors

## 2.5 (3.1)
- [ ] Add gamma (and dpms + idle) support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
https://github.com/swaywm/wlroots/blob/master/examples/idle.c
https://github.com/minus7/redshift/tree/wayland

## 2.X (3.2+)

- [ ] Keep it up to date with possible ddcutil api changes
