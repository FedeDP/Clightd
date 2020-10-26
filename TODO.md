## 4.3

### Gamma
- [x] Add a Gamma.Changed signal
- [x] Allow concurrent changes on different x displays

### DPMS
- [x] Add a Dpms.Changed signal
- [x] Switch to "si" for dpms changed
- [x] Update clight with new interface (to be committed when on master)
- [x] Same as Xorg: try Xorg, wlr and finally drm

#### wl
- [x] Add support for wayland (dpms-client-protocol)
- [ ] test

#### drm 
- [x] Switch to drm for dpms on tty
- [ ] test
- [ ] Document that only card number should be passed
- [ ] Document that on empty string, card0 will be tried

### Gamma
- [x] Split gamma api like dpms: for X, wlr and tty. On tty, use libdrm
- [ ] Document drm param (cardnumber) and default

#### wl
- [x] Add gamma support on wayland (wlr-gamma-control-unstable-v1)
- [ ] Test
- [ ] memleaks

#### drm
- [x] Add gamma support on tty through libdrm (??)
- [ ] Document that only card number should be passed
- [ ] Document that on empty string, card0 will be tried

### Sensor

#### Camera
- [x] Support mjpeg input format through libjpeg

#### Yoctolight
- [x] Fix Yoctolight Sensor
- [x] Avoid infinite loops in yoctolight sensor
- [ ] Document new yoctolight sensor

### Generic
- [x] Add new deps to pkgbuild and build.yaml
 
## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
