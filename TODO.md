## 3.X
- [ ] Keep it up to date with possible ddcutil/libmodule api changes

## 3.5/4.0
- [ ] Add gamma (and dpms + idle) support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
https://github.com/swaywm/wlroots/blob/master/examples/idle.c
https://github.com/swaywm/sway/tree/master/swayidle
- [ ] Couldn't DPMS be just another case for new Idle implementation? Eg: Idle after 45s -> dim screen. Idle after 5mins -> screen off
- [ ] Eventually drop dpms {Set/Get}Timeouts

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep
