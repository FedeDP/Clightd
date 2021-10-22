## 5.5 (6.0)

### Camera
- [x] Add support for a cropping setting parameter: "x=0.4-0.6,y=0.2-0.8"
- [x] Support crop through crop and selection v4l2 api if available
- [x] fallback at manually skipping pixels

### Gamma
- [x] return 0 for Wl gamma Get (sway protocol) even if it is not implemented, to avoid breaking clight

### Backlight
- [x] Never set current pct to -1 before emitting signals; fixes https://github.com/FedeDP/Clight/issues/225
- [x] Route old Backlight module to new Backlight2 to avoid api break for now

### Backlight2
- [x] Create new object paths for each detected display
- [x] Drop "Set/Get" methods, and add a Set method on each object path
- [x] SetAll will call Set on each object path (and will be renamed to Set); same for GetAll
- [x] Follow Keyboard API
- [x] Create this new interface under org.clightd.clightd.Backlight2 (to avoid api break)?
- [x] Actually implement logic
- [x] Add support for monitor hotplug using new ddcutil 1.2.0 api ddca_redetect_displays()
- [x] Fix: internal backlight is not removed when bl_power goes to 4 (no udev event is triggered indeed)... added a note for now in code (not needed probably; let's push with the actual code commented out and remove it later)

- [x] Clight -> avoid specifying that clightd.service must be enabled as it is started by clight later in the process (thus i2c module is already loaded and ddc C api can load external monitors!)
- [ ] Clight -> add faq entry "no external monitor found -> disable clightd unit and let clight start it!"
- [ ] Clight -> Fix: right now clight only emits internal signals for first backlight interface for whom it received a signal; what if that interface disappears? -> subscribe to object_added/removed signals and take a map of available objects!
- [ ] Clight -> On objectadded set state.curr_bl_pct on new monitor, so that runtime added monitors have correct backlight set (and check if any override curve exists!)
- [ ] Clight -> drop conf option "screen_sysname" in clight, now useless (?) as new API set backlight on any found device

- [x] call sd_bus_emit_object_added() sd_bus_emit_object_removed() When object path are created/deleted

- [ ] use more meaningful return types for Set,Raise,Lower? (boolean does not make much sense...)...
- [x] Better error handling/code
- [x] Drop bl_store_vpcode() and only load vpcode from CLIGHTD_BL_VCP env?
- [x] Add CLIGHTD_BL_VCP Environment variable to systemd script with a comment thus it is simple to update it if needed
- [x] Expose Max and Internal properties
- [ ] Update dbus api wiki

### KbdBacklight
- [x] call sd_bus_emit_object_added() sd_bus_emit_object_removed() When object path are created/deleted
- [x] Fix: udev_reference is a snapshot of an udev device at a current time. Wrong!

## 5.x
- [ ] Keep it up to date with possible ddcutil api changes

## 6.x (api break release)

### Generic
- [ ] Drop is_smooth options for gamma
- [ ] avoid returning boolean where it does not make sense
- [ ] Drop old BACKLIGHT module -> in case, drop {Lower,Raise,Set}All from clightd polkit policy
- [ ] Rename Backlight2 to Backlight

### Pipewire
- [ ] merge pipewire work
- [ ] move clightd to user service

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
