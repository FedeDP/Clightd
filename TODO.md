## 3.2
- [ ] add support for GetSensorData android app
- [ ] it will come as bash scripts
- [ ] add a new Sensor with type "SCRIPT"
- [ ] Scripts will be parsed and used by priority -> "99-SensorSteam", "60-x" etc etc
- [ ] "CaptureScript" method
- [ ] Bash scripts will expose a "isAvailable" and a "capture" functions
- [ ] bash functions will write to stdout; called by popen()
- [ ] android apps need screen to be turned on, as otherwise Doze will kill them...state it in the gh wiki
- [ ] ScriptAdded bus signal when a new script is added. Inotify on scripts folder!
- [ ] devname = "Script" or devname = "GSD" (to be used by clight conf if user is willing to force a script)
- [ ] force script timeout from clightd (eg popen("timeout 1 MYSCRIPT") and check if it returns 124 (timed out)

## 3.3
- [ ] Add gamma (and dpms + idle) support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
https://github.com/swaywm/wlroots/blob/master/examples/idle.c
https://github.com/swaywm/sway/tree/master/swayidle
- [ ] Couldn't DPMS be just another case for new Idle implementation? Eg: Idle after 45s -> dim screen. Idle after 5mins -> screen off
- [ ] Eventually drop dpms {Set/Get}Timeouts

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep

## 3.X

- [ ] Keep it up to date with possible ddcutil api changes
