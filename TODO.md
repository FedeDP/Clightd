## 4.3

### DPMS
- [x] Add a Dpms.Changed signal
- [x] Switch to "si" for dpms changed
- [x] Update clight with new interface (to be committed when on master)
- [x] Same as Xorg: try Xorg, wlr and finally drm
- [x] Return "b" in Set method, like gamma and backlight Set
- [x] Split Dpms/Xorg, Dpms/Wl, Dpms/Drm
- [x] Only build dpms_plugins folder if dpms is enabled

#### wl
- [x] Add support for wayland (dpms-client-protocol)
- [x] memleaks

#### drm 
- [x] Switch to drm for dpms on tty

### Gamma
- [x] Add a Gamma.Changed signal
- [x] Allow concurrent changes on different x displays
- [x] Split gamma api like dpms: for X, wlr and tty. On tty, use libdrm
- [x] Split Gamma/Xorg, Gamma/Wl, Gamma/Drm
- [x] Only build gamma_plugins folder if gamma is enabled

#### wl
- [x] Add gamma support on wayland (wlr-gamma-control-unstable-v1)
- [x] Fix: wl_display_disconnect() resets state (add a wl_utils::fetch_display() that internally keeps a map of opened displays and disconnects with map dtor at end of program?)
- [x] fiX "compositor doesn't support wlr-gamma-control-unstable-v1" freeze
- [x] memleaks
- [x] Smooth transitions not working on wayland. Fallback at non-smooth

#### drm
- [x] Add gamma support on tty through libdrm (??)

### Screen
- [x] Split screen api like dpms: for X, wlr...tty??
- [x] Split Screen/Xorg, Screen/Wl, Screen/Drm
- [x] Only build screen_plugins folder if screen is enabled
- [x] Unify cycles to compute frame brightness
- [x] Update apidoc

#### wl
- [x] Add screen support on wayland (wlr-screencopy-unstable-v1)

#### Fb
- [x] https://github.com/GunnarMonell/fbgrab/blob/master/fbgrab.c ??

### Sensor

#### Camera
- [x] Support mjpeg input format through libjpeg
- [x] Avoid setting camera fmt during validation, as device validation should not change device status
- [x] Restore previous camera settings at end of capture

#### Yoctolight
- [x] Fix Yoctolight Sensor
- [x] Avoid infinite loops in yoctolight sensor

### Generic
- [x] Add new deps to pkgbuild and build.yaml
- [x] Document drm param and defaults for gamma and dpms
- [x] Document xauthority naming switch to "env" and its use with XDG_RUNTIME_DIR for wayland for both gamma and dpms
- [x] Document new yoctolight sensor
- [x] Update API docs (API breaks)
- [x] Bump Clightd to 5.0
- [x] Add new deps to wiki (libwayland-dev on ubuntu, wayland on arch and wayland-devel on fedora) + libdrm + libjpeg-turbo + libusb
- [x] Fix: second call to get an open wl_display does not need XDG_RUNTIME_DIR properly set
- [x] Moved to LIBEXECDIR intead of LIBDIR (#52 PR #53)

## 4.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
