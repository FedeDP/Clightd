## 5.4

### Sensor
- [x] ALS sensors (and yoctolight too) have a logarithmic curve for lux values (#71)

### Pipewire
- [x] Support pipewire for Camera sensor? This would allow multiple application sharing camera
- [ ] Support monitor sensor api for pipewire
- [ ] Pipewire run as root needs XDG_RUNTIME_DIR env

## 6.x

### Backlight
- [ ] Create new object paths for each detected display
- [ ] Drop "Set/Get" methods, and add a Set method on each object path
- [ ] SetAll will call Set on each object path (and will be renamed to Set)
- [ ] Drop GetAll method: you can only call Get on each object
- [ ] Follow Keyboard API

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
