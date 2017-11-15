## 1.6

- [x] Disable V4L2_CID_AUTO_WHITE_BALANCE,V4L2_CID_AUTOGAIN,V4L2_CID_BACKLIGHT_COMPENSATION for camera
- [x] use ddcutil for external monitor? (http://www.ddcutil.com/) https://github.com/rockowitz/ddcutil/blob/master/src/sample_clients/demo_get_set_vcp.c (it has a C api)
- [x] setbrightness to get percentage instead of value (eg: setbrightness "" 0.5), this way it can set correct brightness level across multiple monitors
- [x] make setbrightness available on desktop pc too (now it won't work as it won't find any /sys/class/backlight device)
- [x] Update readme
- [x] drop isinterfaceenabled
- [ ] fix memleak
- [x] add a modules-load.d file to load i2c-dev module to be manually installed for users (add a description in the wiki)
- [x] make ddcutil support optional
- [x] update aur
