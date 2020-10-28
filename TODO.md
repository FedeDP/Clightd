## 4.3

### Backlight
- [x] Drop is_smooth param

### DPMS
- [x] Add a Dpms.Changed signal
- [x] Switch to "si" for dpms changed
- [x] Update clight with new interface (to be committed when on master)
- [x] Same as Xorg: try Xorg, wlr and finally drm
- [x] Return "b" in Set method, like gamma and backlight Set

#### wl
- [x] Add support for wayland (dpms-client-protocol)
- [x] memleaks

#### drm 
- [x] Switch to drm for dpms on tty

### Gamma
- [x] Add a Gamma.Changed signal
- [x] Allow concurrent changes on different x displays
- [x] Split gamma api like dpms: for X, wlr and tty. On tty, use libdrm
- [x] Drop is_smooth param

#### wl
- [x] Add gamma support on wayland (wlr-gamma-control-unstable-v1)
- [x] Fix: wl_display_disconnect() resets state (add a wl_utils::fetch_display() that internally keeps a map of opened displays and disconnects with map dtor at end of program?)
- [x] fiX "compositor doesn't support wlr-gamma-control-unstable-v1" freeze
- [x] memleaks
- [ ] Fix smooth transitions not working... ("The gamma ramps don't have the correct size", see https://github.com/swaywm/wlroots/blob/master/types/wlr_gamma_control_v1.c#L90)

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
- [x] Document new yoctolight sensor
- [ ] Update API docs (DPMS/gamma/backlight API breaks)
- [ ] Add Gamma/Xorg, Gamma/Wl, Gamma/Drm, same for DPMS, object paths to force a certain backend
- [ ] Check how to port SCREEN on wl and tty too?
- [x] Bump Clightd to 5.0
- [ ] Add new deps to wiki (libwayland-dev on ubuntu, wayland on arch and wayland-devel on fedora)

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
