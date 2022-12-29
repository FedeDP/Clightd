## 5.8

### Backlight
- [x] Add support for brightness emulation through gamma 
- - [x] Xorg: fix naming differing from /sys/class/drm one... :/
- - [x] drm
- - [x] wl
- [x] Fix xorg gamma Get (account for known brightness!)
- [ ] Document CLIGHTD_XORG_TO_DRM env variable!
- [x] Split backlight in plugins: sysfs/ddc (xrandr or wlrandr or drm(?)) Init,set,get api
- [x] only build the new features if required libs are present, just like we do for gamma
- [x] fix gamma.Set was not setting gamma on all screens for Xorg
- [x] Add an env variable to disable emulated backlight, similar to BL_VCP_ENV
- [x] Check all /sys/class/drm/cardX available, not just card0!

## 6.0 (api break)

### Backlight
- [ ] Drop old backlight API
- [ ] Move Backlight2 to Backlight
- [ ] Drop {Lower,Raise,Set}All from clightd polkit policy

### Generic
- [ ] Drop is_smooth options for gamma
- [ ] avoid returning boolean where it does not make sense
- [ ] port CI to gh actions

## 6.1

### Pipewire
- [ ] Check pipewire 0.3.60 v4l2 changes 

## 6.x

### Backlight
- [ ] Keep it up to date with possible ddcutil api changes

### Pipewire
- [ ] Fix set_camera_setting() impl -> how to get current value? how to set a new value?

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
