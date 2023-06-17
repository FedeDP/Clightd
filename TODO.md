## 5.x

### CI

- [ ] Support prebuilt artifacts (deb, rpm) build from as old as possible glibc version to reach widest compatibility
- [ ] both master devel and release

### Pipewire
- [ ] Fix set_camera_setting() impl -> how to get current value? how to set a new value?
- [ ] Check pipewire 0.3.60 v4l2 changes 
- [x] fix source of segfault
- [x] fix node names, ose object.path instead of object id 

### Camera 
- [x] Do not even try to open devices without correct ID_V4L_CAPABILITIES

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
