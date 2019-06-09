## 4.0

### Generic
- [x] Fix issues with timerfd_settime for long timeouts
- [x] Add issue template
- [x] Update to new ddcutil 0.9.5 interface. Require it.
- [x] Bump to 4.0: api break

### Idle
- [x] Avoid depending on X
- [x] IDLE support on wayland
- [x] Update API doc

### Dpms
- [x] Drop DPMS Set/getTimeouts, and only use get/set 
- [x] Add support for dpms on tty
- [x] Update doc

### CPack
- [x] Add Cpack support to cmake
- [x] Fix cpack Dependencies on ubuntu
- [x] Fix cpack: only add enabled dependencies

### Camera
- [x] Improve camera.c code
- [x] Reduce camera.c logging
- [x] Support Grayscale pixelformat for CAMERA sensor
- [x] Fix #24
- [ ] Improve camera brightness compute with a new histogram-based algorithm (#25)

### Backlight
- [x] Support DDCutil DDCA_Display_Info path.io_mode (eg: /dev/i2c-%d) as uid (as SerialNumber can be null/empty on some devices) (Fix #27)

## 4.1

#### Gamma
- [ ] Add gamma support on wayland (??)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c

#### Dpms
- [ ] Add support for dpms on wayland(??)

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
