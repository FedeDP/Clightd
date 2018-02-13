## 2.0

- [x] Disable V4L2_CID_AUTO_WHITE_BALANCE,V4L2_CID_AUTOGAIN,V4L2_CID_BACKLIGHT_COMPENSATION for camera
- [x] use ddcutil for external monitor? (http://www.ddcutil.com/) https://github.com/rockowitz/ddcutil/blob/master/src/sample_clients/demo_get_set_vcp.c (it has a C api)
- [x] setbrightness to get percentage instead of value (eg: setbrightness "" 0.5), this way it can set correct brightness level across multiple monitors
- [x] Update readme
- [x] drop isinterfaceenabled
- [x] check for memleaks
- [x] add a modules-load.d file to load i2c-dev module to be manually installed for users (add a description in the wiki)
- [x] make ddcutil support optional
- [x] update aur
- [x] add getbrightnesspct and setbrightnesspct functions
- [x] add setbrightnesspct_external method, getbrightnesspct_external method (return array for each monitor of their brightness level)
- [x] add setbrightness_external and getbrightness_external methods
- [x] drop getactualbrightness method
- [x] add a version property, clight will check this when it starts up
- [x] setbrightnesspct_all should always be present, and just fallback to setbrightnesspct if ddcutil is not built
- [x] update README: bus interface
- [x] update README ddcutil
- [x] in archlinux PKGBUILD, automatically install i2c_clightd.conf file in /etc/modules-load.d/
- [x] update to new 0.8.7 ddcutil API

- [x] fix setallbrightness
- [ ] implement set_brightness
- [x] implement set gamma smooth
- [x] drop gamma_smooth, backlight_smooth and dimmer_smooth in clight
- [x] install ddcutil module's load file if "make install WITH_DDC=1"
- [ ] better error checking in backlight.c

- [x] test
- [ ] new release
