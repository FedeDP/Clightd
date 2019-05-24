## 3.5

### Generic
- [x] Fix issues with timerfd_settime for long timeouts
- [x] Add issue template
- [x] Update to new ddcutil 0.9.5 interface. Require it.

### Dimmer
- [x] Avoid depending on X
- [x] IDLE support on wayland (wlroots)
- [ ] Update API doc

### Dpms
- [ ] Drop DPMS Set/getTimeouts, and only use get/set 
- [ ] Add support for dpms on wlroots (wlr_output_enable(false)) -> depend upon wlroots library, optional dep
- [ ] Add support for dpms on tty (https://github.com/bircoph/suspend/blob/master/vbetool/vbetool.c#L356) -> depends on libx86
Even better, on tty: https://github.com/karelzak/util-linux/blob/master/term-utils/setterm.c#L730

### CPack
- [x] Add Cpack support to cmake
- [x] Fix cpack Dependencies on ubuntu
- [x] Fix cpack: only add enabled dependencies

### Camera
- [x] Improve camera.c code
- [x] Reduce camera.c logging
- [x] Support Grayscale pixelformat for CAMERA sensor
- [ ] Fix frame capturing parameters...they are not applied immediately (at least on my webcam)
- [ ] Fix #24

## 3.6
- [ ] Drop privileges and only gain them when really needed (check repo history, there already was an attempt)

## 3.7/4.0
- [ ] Add gamma support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c

## 3.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
