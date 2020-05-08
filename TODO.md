## 4.1

#### Sensor

- [x] Properly fix support for ALS sensors (right now only acpi-als is supported)
- [x] Support settings string for ALS sensor (interval, min and max)
- [x] Document new sensor settings
- [x] Add a Custom sensor to fetch data from user provided scripts
- [x] Use validate_dev() even after fetch_dev(): when user passes is a fixed sysname to be used, we must assure it is a correct device
- [x] Add YoctoLight sensor support
- [x] Use validate() function to check for webcams with correct capabilities and start opening it?

- [x] Sensor.Capture method to return number of correctly captured frames

- [x] Document new Custom sensor

- [x] ALlow to call Backlight.Get/Set without specifying a serial id

- [x] Mask Yoctolight for 4.1 release (not yet ready)

- [x] Fix bug in GetAll introduced recently

- [ ] Release!

#### Bugfix

- [x] Kill clightd on bus connection closed/reset
- [x] Provide a way to customize ddcutil vp code
- [x] Provide a way to customize ddcutil vp code through bash env

## 4.2

- [ ] Fix Yoctolight Sensor
- [ ] Avoid infinite loops in yoctolight sensor
- [ ] Document new yoctolight sensor

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

#### Gamma
- [ ] Add gamma support on wayland (??)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c

#### Dpms
- [ ] Add support for dpms on wayland(??)

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
