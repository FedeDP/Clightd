## 1.4
- [ ] return an array of double with luminosity values for each captured frame. Client will then need to process this array as they wish (better customization)

## 1.5
- [ ] Add an udev monitor on each backlight interface "enabled" value and send a signal as soon as it changes (this way clight can do a capture as soon as an interface became enabled by hooking on this signal)
- [ ] check for memleaks?
