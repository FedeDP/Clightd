## 4.3

### Gamma
- [x] Add a Gamma.Changed signal
- [x] Allow concurrent changes on different x displays

### DPMS
- [x] Add a Dpms.Changed signal

### Sensor

#### Camera
- [x] Support mjpeg input format through libjpeg

#### Yoctolight
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
