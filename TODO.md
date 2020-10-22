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
- [x] Fix Yoctolight Sensor
- [x] Avoid infinite loops in yoctolight sensor
- [ ] Document new yoctolight sensor

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

#### Gamma
- [ ] Add gamma support on wayland (??)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
- [ ] Use a double for gamma value too (eg: between 0 and 10000), -> 0.65 default value (6500) -> gamma protocol for wlroots uses that 
- [ ] Split gamma api like dpms: for X, wlroots and tty. On tty, use libdrm

#### Dpms
- [ ] Add support for dpms on wayland(??)

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
