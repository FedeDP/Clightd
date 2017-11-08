## 1.6

- [x] Disable V4L2_CID_AUTO_WHITE_BALANCE,V4L2_CID_AUTOGAIN,V4L2_CID_BACKLIGHT_COMPENSATION for camera (https://www.linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec/ch01s08.html)

## Later
It seems my driver does not send proper udev events for this to be implemented soon...  
Code is already in place but commented out.  

- [ ] Add an udev monitor on each backlight interface "enabled" value and send a signal as soon as it changes (this way clight can do a capture as soon as an interface became enabled by hooking on this signal)
