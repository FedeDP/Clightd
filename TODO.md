## 3.0

### ALS Support
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
- [x] Add back support for multiple sensor captures (useful for webcam)
- [x] Properly return value between 0 and 1.0 in CaptureWebcam
- [x] Updated api Doc
- [x] Fix udev monitors interface
- [x] Leave brightness_smooth_cb if no internal backlight is present
- [x] Udev monitors must be unref'd!

### New Api (org.clightd.clightd)
- [x] Change interface to org.clightd.clightd
- [x] Every feature will have its own object path, eg: /org/clightd/clightd/Backlight {Set,Get}
- [x] Sensor interface becomes: /org/clightd/clightd/Sensor org.clightd.clightd.Sensor {Capture, IsAvailable} + /org/clightd/clightd/Sensor/Als {Capture, IsAvailable} + /org/clightd/clightd/Sensor/Webcam {Capture, IsAvailable}
- [x] Other becomes eg: /org/clightd/clightd/Backlight org.clightd.clightd.Backlight {Set/Get}
- [x] Gamma and Backlight smooth should be equal (gamma checks if smooth is enabled and has 2 different behaviours)
- [x] Add some more MODULE_INFO
- [x] Valgrind check
- [x] Cleanup includes etc etc
- [x] Fix build with no gamma/dpms/idle...
- [x] Switch to libmodule
- [x] Sensor ctor should have priority 101, not 100 + modules_quit return err type (Wait for 3.0.0 release of libmodule)
- [x] fix "Failed to stop module." error when leaving

### New Idle interface
- [x] Clightd will emit a signal (with ClientX as destination) when the timeout is reached/left. On X it will be just like dimmer clight module does now. On wayland it will use idle protocol (possibly later)
- [x] It will support multiple clients
- [x] callback when changing idle client timeout: it should get current elapsed time and reset its current timer based on that (as clight does)
- [x] method_rm_client should remove vtable too!
- [x] Valgrind check!
- [x] Validate for Idle Client properties setters
- [x] Rename "Xauthority" prop to more generic "AuthCookie"

### Doc
- [ ] Update API reference
- [x] Update any org.clightd.backlight reference

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

## 3.2
- [ ] Add gamma (and dpms + idle) support on wayland (wlroots)
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
https://github.com/swaywm/wlroots/blob/master/examples/idle.c
https://github.com/swaywm/sway/tree/master/swayidle
- [ ] Is dpms supported? Couldn't it be just another case for new Idle implementation? Eg: Idle after 45s -> dim screen. Idle after 5mins -> screen off.

## Ideas
- [ ] follow ddcci kernel driver and in case, drop ddcutil and add the kernel driver as clightd opt-dep

## 3.X

- [ ] Keep it up to date with possible ddcutil api changes
