## 1.3
- [x] Add a "isBacklightInterfaceEnabled" method in clightd that checks if given interface (or first interface found, if parameter is null) is enabled (check clight todo for more info)
- [x] switch to poll with a signalfd to manage external signals

## Later
- [ ] Add an udev monitor on each backlight interface "enabled" value and send a signal as soon as it changes (this way clight can do a capture as soon as an interface became enabled by hooking on this signal)
