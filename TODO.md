## 5.X

### Sensor
- [x] ALS sensors (and yoctolight too) have a logarithmic curve for lux values (#71)
- [x] fixed memleak in camera sensor

### KbdBacklight
- [x] Add Get and GetTimeout method on main object to fetch all kbd backlight and timeouts alltoghether

## 6.0

### Backlight
- [ ] Create new object paths for each detected display
- [ ] Drop "Set/Get" methods, and add a Set method on each object path
- [ ] SetAll will call Set on each object path (and will be renamed to Set); same for GetAll
- [ ] Follow Keyboard API

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
