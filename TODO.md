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
- [x] Instead of 30s sleep, use an udev_monitor on drm subsystem?

### KbdBacklight
- [x] call sd_bus_emit_object_added() sd_bus_emit_object_removed() When object path are created/deleted
- [x] Fix: udev_reference is a snapshot of an udev device at a current time. Wrong!

### ALS
- [x] Fix: avoid using cached udev_dev reference in loop (thus always returning same ambient brightness read during a Capture request)
- [x] Fixed EIO errors

### Sensor
- [x] Only emit Sensor.Changed signal for added/removed devices
- [ ] Add a List method in Sensor api to list all available devices ("as")

### Pipewire
- [x] Support pipewire for Camera sensor? This would allow multiple application sharing camera
- [x] Pipewire run as root needs XDG_RUNTIME_DIR env -> workaround: find the first /run/user folder and use that
- [ ] Unify camera settings between camera and pipewire sensors... ?
- [x] Support monitor sensor api for pipewire
- [x] Fix segfault
- [x] Fix subsequent Capture
- [ ] Check if installing it on system causes pipewire module to be disabled because clightd starts before /run/user/1000 is created!
-> in case, disable monitor for now and instead rely upon user-provided interface string or PW_ID_ANY
- [ ] Use caller uid instead of defaulting to first found user during Capture!
- [x] Use a map to store list of nodes?
- [x] Free list of nodes upon exit!
- [ ] Fix xdg_runtime_dir set to create monitor
- [ ] Fix memleaks

### Generic
- [x] When built with ddcutil, clightd.service should be started after systemd-modules-load.service
- [x] Show commit hash in version
- [ ] All api that require eg Xauth or xdg rutime user, fallback at automatically fetching a default value given the caller:
> unsigned int uid;
//     sd_bus_creds *c;
//     sd_bus_query_sender_creds(m, SD_BUS_CREDS_EUID, &c);
//     sd_bus_creds_get_euid(c, &uid); (/run/user/1000)
// Function fill_bus_creds() -> fills a global "runtime_dir", "xauth_dir" etc etc given a sd_bus_message, then exposes "sender_get_runtime_dir() etc etc"

## 5.x
- [ ] Keep it up to date with possible ddcutil api changes

## 6.x (api break release)

### Generic
- [ ] Drop is_smooth options for gamma
- [ ] avoid returning boolean where it does not make sense
- [ ] Drop old BACKLIGHT module -> in case, drop {Lower,Raise,Set}All from clightd polkit policy
- [ ] Rename Backlight2 to Backlight

### Move to user service (?)
- [ ] move clightd to user service
- [ ] Drop polkit and use sd_session_is_active() 
- [ ] Add udev rules for yoctolight, als, keyboard and backlight modules for "clightd" 
- [ ] Drop useless API params (eg: DISPLAY, XAUTHORITY, XDG_RUNTIME_DIR etc etc)
- [ ] # groupadd clightd
- [ ] # usermod -aG clightd myusername
- [ ] # echo 'KERNEL=="i2c-[0-9]*", GROUP="clightd"' >> /etc/udev/rules.d/10-local_i2c_group.rules
... would break https://github.com/FedeDP/Clight/issues/144 ...

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
