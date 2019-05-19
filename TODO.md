## 3.5
- [x] Fix #24
- [x] Support greyscale pixelformat for CAMERA sensor
- [x] Fix issues with timerfd_settime for long timeouts
- [x] Add Cpack support to cmake
- [x] Reduce camera.c logging
- [x] Add issue template
- [x] Improve camera.c code
- [x] Add support for Grayscale capturing too
- [x] Update to new ddcutil 0.9.5 interface. Require it.

- [ ] Fix frame capturing parameters...they are not applied immediately (at least on my webcam)

## 3.6
- [ ] Drop privileges and only gain them when really needed (check repo history, there already was an attempt)
- [ ] IDLE support on wayland (wlroots)
- [ ] Drop DPMS Set/getTimeouts, and only use get/set + add support for dpms on wlroots? (wlr_output_enable(false) on wlroots)

For reference:  
https://github.com/swaywm/wlroots/blob/master/examples/idle.c  
https://github.com/swaywm/sway/tree/master/swayidle  

## 3.7/4.0
- [ ] Add gamma support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c

## 3.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
