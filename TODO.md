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
