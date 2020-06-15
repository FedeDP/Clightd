## 4.2

### Sensor
- [ ] Fix Yoctolight Sensor
- [ ] Avoid infinite loops in yoctolight sensor
- [ ] Document new yoctolight sensor

### Backlight
- [x] Add a backlight Changed signal in Clightd (Then hook the signal to update state.current_bl. Easy for laptop's internal monitor; impossibile with ddcutil, but possible through ddcci-kernel-driver only for software changes, not hardware ones (ie: through monitor buttons))

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

#### Gamma
- [ ] Add gamma support on wayland (??)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c

#### Dpms
- [ ] Add support for dpms on wayland(??)

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
