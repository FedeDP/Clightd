## 3.0
- [x] Als support
- [x] Use ALS_SUBSYSTEM and CAMERA_SUBSYSTEM where needed (put them in their headers)
- [x] Add a common interface for both camera and als (to be used by clight)
- [x] For Als, properly check that dev->name == "acpi-als" as iio subsystem covers much more sensors
- [x] Move to gh wiki pages
- [x] Rename methods with camelcase
- [x] Fix for CaptureSensor method
- [x] Change directory structure -> sensors/{als.c,camera.c}, utils/{udev.c,polkit.c}, bus/{backlight,dpms,gamma,idle}. Rename clightd.c in main.c
- [x] CaptureWebcam should take a single frame and return a single double, adhering to CaptureSensor interface (ie: require a "s" and returning a "d")
- [x] Updated org.clightd.backlight policy
- [x] Drop setBrightness and getBrightness (useless methods) and rename setAll and getAll -> drop them from org.clightd.backlight policy too!
- [x] Document new API
- [x] IsXAvailable should take a "s" (interface) parameter
- [x] IsXAvailable and CaptureX to return name of used device too
- [x] unify sensor capture return code for all sensors (return double value)
- [x] Drop webcam support for multiple frames capture
- [x] Update api doc

## 3.1
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


## Ideas
- [ ] Add gamma (and dpms + idle) support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
https://github.com/swaywm/wlroots/blob/master/examples/idle.c
https://github.com/minus7/redshift/tree/wayland

- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep

## 3.X

- [ ] Keep it up to date with possible ddcutil api changes
