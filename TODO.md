## 4.1

#### Sensor

- [x] Properly fix support for ALS sensors (right now only acpi-als is supported)
- [x] Support settings string for ALS sensor (interval, min and max)
- [x] Document new sensor settings
- [x] Add a Custom sensor to fetch data from user provided scripts
- [x] Use validate_dev() even after fetch_dev(): when user passes is a fixed sysname to be used, we must assure it is a correct device
- [x] Add YoctoLight sensor support
- [x] Use validate() function to check for webcams with correct capabilities and start opening it?

- [x] Document new Custom sensor

- [ ] Release!

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

#### Gamma
- [ ] Add gamma support on wayland (??)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c

#### Dpms
- [ ] Add support for dpms on wayland(??)

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
