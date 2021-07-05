## 5.4

### Sensor
- [x] ALS sensors (and yoctolight too) have a logarithmic curve for lux values (#71)

### Pipewire
- [x] Support pipewire for Camera sensor? This would allow multiple application sharing camera
- [x] Support monitor sensor api for pipewire
- [ ] Pipewire run as root needs XDG_RUNTIME_DIR env ...
- [ ] Unify camera settings between camera and pipewire sensors

### Run as user?

- [ ] API breaking change (6.0)
- [ ] Connect to user bus
- [ ] Check each module:
- - [x] Signal
- - [x] Bus
- - [x] Pipewire/Camera (require "video" group)
- - [ ] yoctolight
- - [ ] ALS
- - [x] dpms (double check drm)
- - [x] gamma (double check drm)
- - [x] idle
- - [ ] keyboard
- - [ ] backlight
- - [x] screen (double check drm)

- [ ] Drop polkit and use sd_session_is_active() 
- [ ] Add udev rules for yoctolight, als, keyboard and backlight modules for "clightd" group
- [ ] Drop useless API params (eg: DISPLAY, XAUTHORITY, XDG_RUNTIME_DIR etc etc)

... would break https://github.com/FedeDP/Clight/issues/144 ...

## 6.x

### Backlight
- [ ] Create new object paths for each detected display
- [ ] Drop "Set/Get" methods, and add a Set method on each object path
- [ ] SetAll will call Set on each object path (and will be renamed to Set)
- [ ] Drop GetAll method: you can only call Get on each object
- [ ] Follow Keyboard API

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
