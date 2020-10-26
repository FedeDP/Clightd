## 4.3

### DPMS
- [x] Add a Dpms.Changed signal
- [x] Switch to "si" for dpms changed
- [x] Update clight with new interface (to be committed when on master)
- [x] Same as Xorg: try Xorg, wlr and finally drm

#### wl
- [x] Add support for wayland (dpms-client-protocol)
- [x] test -> Get working, Set freeze (see below)
- [ ] Fix wl_display_dispatch blocking
- [x] memleaks

#### drm 
- [x] Switch to drm for dpms on tty

### Gamma
- [x] Add a Gamma.Changed signal
- [x] Allow concurrent changes on different x displays
- [x] Split gamma api like dpms: for X, wlr and tty. On tty, use libdrm

#### wl
- [x] Add gamma support on wayland (wlr-gamma-control-unstable-v1)
- [ ] Test
- [ ] Fix wl_display_dispatch blocking
- [x] fiX "compositor doesn't support wlr-gamma-control-unstable-v1" freeze
- [x] memleaks

#### drm
- [x] Add gamma support on tty through libdrm (??)

### Sensor

#### Camera
- [x] Support mjpeg input format through libjpeg

#### Yoctolight
- [x] Fix Yoctolight Sensor
- [x] Avoid infinite loops in yoctolight sensor

### Generic
- [x] Add new deps to pkgbuild and build.yaml
- [ ] Document drm param (cardnumber) and default for gamma and dpms
- [ ] Document xauthority naming switch to "env" and its use with XDG_RUNTIME_DIR for wayland for both gamma and dpms
- [ ] Document new yoctolight sensor

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
