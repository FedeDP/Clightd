## 5.x

### Als
- [x] add support for buffer als reads

### Pipewire
- [ ] Fix set_camera_setting() impl -> how to get current value? how to set a new value?
- [ ] Check pipewire 0.3.60 v4l2 changes 
- [x] fix source of segfault
- [x] fix node names, ose object.path instead of object id 
- [x] use same algorithm as camera sensor to retrieve default camera (ie: the one with highest USEC_INITIALIZED property)

### Camera 
- [x] Do not even try to open devices without correct ID_V4L_CAPABILITIES
- [x] use USEC_INITIALIZED property instead of sysnum

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
