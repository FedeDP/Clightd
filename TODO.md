## 1.3
- [ ] Add a "isBacklightInterfaceEnabled" method in clightd that checks if given interface (or first interface found, if parameter is null) is enabled (check clight todo for more info)
- [ ] play with udevadm info /sys/class/backlight/intel_backlight -a | grep dpms -> what if we can check dpms state from udev? (it would ideally work on both x and wayland; moreover we could set a monitor on dpms attribute (for 1.4 release, see below) and send a signal when it changes)

## Later
- [ ] Add an udev monitor on each backlight interface "enabled" value and send a signal as soon as it changes (this way clight can do a capture as soon as an interface became enabled by hooking on this signal)
