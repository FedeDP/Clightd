## 5.8

### Backlight
- [x] Add support for brightness emulation through gamma 
- - [x] Xorg: fix naming differing from /sys/class/drm one... :/
- - [x] drm
- - [x] wl
- [x] Fix xorg gamma Get (account for known brightness!)
- [x] Split backlight in plugins: sysfs/ddc (xrandr or wlrandr or drm(?)) Init,set,get api
- [x] only build the new features if required libs are present, just like we do for gamma
- [x] fix gamma.Set was not setting gamma on all screens for Xorg
- [x] Add an env variable to disable emulated backlight, similar to BL_VCP_ENV
- [x] add an env to disable ddc or sysfs too!
- [x] Check all /sys/class/drm/cardX available, not just card0!
- [ ] Document new env variables!

### Screen
- [x] fix `rgb_frame_brightness` impl
- [x] fix xorg impl

### Dpms
- [x] implement wlroots DPMS protocol (wlr-output-power-management-unstable-v1.xml)
- [x] give higher priority to wlroots protocol (in respect to kwin protocol)
- [x] rename kwin protocol to `kwin_wl` 
- [x] `wl` is instead wlroots (like all other plugins)
- [x] fix KWin_wl
- [ ] update wiki pages (new object path + wlroots as default under `Wl` obj path)

### Generic
- [x] port CI to gh actions

## 5.x

### Pipewire
- [ ] Fix set_camera_setting() impl -> how to get current value? how to set a new value?
- [ ] Check pipewire 0.3.60 v4l2 changes 

### Backlight
- [ ] Keep it up to date with possible ddcutil api changes

## 6.0 (api break)

### Backlight
- [ ] Drop old backlight API
- [ ] Move Backlight2 to Backlight
- [ ] Drop {Lower,Raise,Set}All from clightd polkit policy

### Generic
- [ ] Drop is_smooth options for gamma
- [ ] avoid returning boolean where it does not make sense

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
