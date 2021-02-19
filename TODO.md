## 5.2

### Backlight
- [x] Support ddcutil 1.0.0
- [x] Emit Backlight.Changed for external monitors
- [x] Improve external monitors ddc handling

### Keyboard
- [x] Add a keyboard backlight module

### Idle
- [x] Check auth in GetClient

## 5.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## 6.0

### Backlight
- [ ] Create new object paths for each detected display
- [ ] Drop "Set/Get" methods, and add a Set method on each object path
- [ ] SetAll will call Set on each object path (renamed to Set)
- [ ] Drop GetAll method: you can only call Get on each object
- [ ] Follow Keyboard API

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
