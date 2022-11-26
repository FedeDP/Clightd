## 6.0

### Backlight
- [x] Drop old backlight API
- [x] Move Backlight2 to Backlight
- [x] Drop {Lower,Raise,Set}All from clightd polkit policy
- [ ] Add support for brightness emulation through gamma -> fix Xorg naming differing from /sys/class/drm one... :/
- [x] drm
- [ ] wl
- [x] Fix xorg gamma Get (account for known brightness!)
- [ ] Split backlight in plugins: sysfs/ddc/emulated (xrandr or wlrandr or drm(?)) Init,set,get api

### Generic
- [ ] Drop is_smooth options for gamma
- [ ] avoid returning boolean where it does not make sense

## 6.x

### Backlight
- [ ] Keep it up to date with possible ddcutil api changes

### Pipewire
- [ ] Fix set_camera_setting() impl -> how to get current value? how to set a new value?

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
