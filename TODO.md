## 5.x
- [ ] Keep it up to date with possible ddcutil api changes

### Camera 
- [x] Fix capture -> avoid setting priority to a low value

### Pipewire
- [ ] Fix set_camera_setting() impl -> how to get current value? how to set a new value?

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
