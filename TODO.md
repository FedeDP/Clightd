## 5.X
- [x] Keep it up to date with possible ddcutil/libmodule api changes

### Backlight
- [ ] Create new object paths for each detected display
- [ ] Drop "Set/Get" methods, and add a Set method on each object path
- [ ] SetAll will call Set on each object path (renamed to Set)
- [ ] Drop GetAll method: you can only call Get on each object

### Keyboard
- [x] Add a keyboard backlight module

### Idle
- [x] Check auth in GetClient/DestroyClient

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
