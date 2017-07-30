## 1.4
- [x] return an array of double with luminosity values for each captured frame. Client will then need to process this array as they wish (better customization)
- [x] fix issues in camera.c
- [x] improve camera.c
- [x] return a 'b' from isbacklightinterfaceenabled
- [x] setdpms with -1 as new state will disable dpms 

## 1.5
- [ ] Add an udev monitor on each backlight interface "enabled" value and send a signal as soon as it changes (this way clight can do a capture as soon as an interface became enabled by hooking on this signal)
- [ ] check for memleaks?
