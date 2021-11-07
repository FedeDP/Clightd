## 5.5

### Camera
- [x] Add support for a cropping setting parameter: "x=0.4-0.6,y=0.2-0.8"
- [x] Support crop through crop and selection v4l2 api if available
- [x] fallback at manually skipping pixels

### Gamma
- [x] return 0 for Wl gamma Get (sway protocol) even if it is not implemented, to avoid breaking clight
- [x] gamma on sway fix: keep connection alive and call dispatch!
- [x] fix segfault 
- [x] move free(priv) inside each plugin as sway implementation requires it to be kept alive!

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

- [x] call sd_bus_emit_object_added() sd_bus_emit_object_removed() When object path are created/deleted

- [x] Better error handling/code
- [x] Drop bl_store_vpcode() and only load vpcode from CLIGHTD_BL_VCP env?
- [x] Add CLIGHTD_BL_VCP Environment variable to systemd script with a comment thus it is simple to update it if needed
- [x] Expose Max and Internal properties
- [ ] Update dbus api wiki
- [x] add a page about monitor hotplugging (dep on ddcutil >= 1.2.0 and refresh time!)
- [x] Investigate memleaks (related to ddca_redetect_displays()?) -> see here: https://github.com/rockowitz/ddcutil/issues/202
- [ ] Instead of 30s sleep, use an udev_monitor on drm subsystem?

### KbdBacklight
- [x] call sd_bus_emit_object_added() sd_bus_emit_object_removed() When object path are created/deleted
- [x] Fix: udev_reference is a snapshot of an udev device at a current time. Wrong!

### Generic
- [x] When built with ddcutil, clightd.service should be started after systemd-modules-load.service
- [x] Show commit hash in version

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
