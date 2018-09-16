# Clightd

[![Build Status](https://travis-ci.org/FedeDP/Clightd.svg?branch=master)](https://travis-ci.org/FedeDP/Clightd)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/7563e6f8e83f4b1aa884c6032709e341)](https://www.codacy.com/app/FedeDP/Clightd?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=FedeDP/Clightd&amp;utm_campaign=Badge_Grade)

Clightd is a bus interface that lets you easily set screen brightness, gamma temperature and get ambient brightness through webcam frames capture.

**Please note that in Ubuntu 16.04 polkit.service was named polkitd.service. You must manually change the "Require=" line in clightd.service.**

## It currently needs:
* libsystemd >= 221 (systemd/sd-bus.h) or elogind (elogind/sd-bus.h)
* libudev or libeudev (libudev.h)

### Needed only if built with gamma support:
* libxrandr (X11/extensions/Xrandr.h)
* libx11 (X11/Xlib.h)

### Needed only if built with dpms support:
* libxext (X11/extensions/Xext.h)
* libx11 (X11/Xlib.h)

### Needed only if built with idle time support:
* libxss (X11/extensions/scrnsaver.h)
* libx11 (X11/Xlib.h)

### Needed only if built with ddcutil support:
* ddcutil >= 0.9.0 (ddcutil_c_api.h)

## Runtime deps:
* shared objects from build libraries
* polkit

## Build time switches:
* DISABLE_GAMMA=1 (to disable gamma support)
* DISABLE_DPMS=1 (to disable dpms support)
* DISABLE_IDLE=1 (to disable user idle time support)
* DISABLE_DDC=1 (to disable [ddcutil](https://github.com/FedeDP/Clightd#ddcutil-support) support)

## Build instructions:
Build and install:

    $ make
    # make install

Uninstall:

    # make uninstall

**It is fully valgrind and cppcheck clean.**

### Valgrind is run with:

    $ alias valgrind='valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-leak-kinds=all -v'

### Cppcheck is run with:

    $  cppcheck --enable=style --enable=performance --enable=unusedFunction

## Devel info
Clightd brightness API was primarily developed with laptops in mind; it does now support desktop PCs too, through ddcutil.  

For laptops, brightness related bus interface methods make all use of libudev to write and read current values (no fopen or other things like that).  
All method calls use libudev to take correct device path, and fallback to first subsystem matching device if empty string is passed.  
For desktop PCs, ddcutil gets used to change external monitor brightness. For more info, see [ddcutil](https://github.com/FedeDP/Clightd/tree/ddcutil#ddcutil-support) section below.  

Getgamma function supports 50-steps temperature values. It tries to fit temperature inside a 50 step (eg: it finds 5238, tries if 5200 or 5250 are fine too, and in case returns them. Otherwise, it returns 5238.)  

Clightd makes use of polkit: active sessions only can call polkit-protected methods.  

You may ask why did i developed this solution. The answer is quite simple: on linux there is no simple and unified way of changing screen brightness.  
So, i thought it could be a good idea to develop a bus service that can be used by every other program.  

My idea is that anyone can now implement something similar to [clight](https://github.com/FedeDP/Clight) without messing with videodev/libudev and code in general.  
A clight replacement, using clightd, can be something like (pseudo-code):

    $ ambient_br = busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight captureframes "si" "" 5
    $ avg_br_percent = compute_avg(ambient_br, 5)
    $ busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight setallbrightness "d(bdu)s" avg_br_percent 1 0.05 80 ""
    
Last line will smoothly change current backlight on every internal/external screen it finds.

**Note that passing an empty/NULL string as internal backlight interface parameter will make clightd use first subsystem matching device it finds (through libudev).** It should be good to go in most cases.

## Bus interface

Please note that for internal laptop screen, serialNumber must be your backlight kernel interface (eg: intel_backlight) or an empty string (to forcefully use first udev backlight subsystem matching device).  

### Methods

*Smooth struct* here means:  
* b -> isSmooth (true for smooth change)
* d (u for gamma) -> smooth step (eg: 0.02 for brightness, or 50 for gamma)
* u -> smooth timeout: timeout for smoothing

| Method | IN | IN values | OUT | OUT values | Polkit restricted | X only |
|-|:-:|-|:-:|-|:-:|-|
| getbrightness | as | <ul><li>Array of screen serialNumbers</li></ul> | a(sd) | Array of struct with serialNumber and current backlight pct for each desired screen | | |
| getallbrightness | s | <ul><li>Backlight interface for internal monitor</li></ul> | a(sd) | Array of struct with serialNumber and current backlight pct for each screen | | |
| setbrightness | d(bdu)as | <ul><li>Target pct</li><li>Smooth struct</li><li>Array of screen serialNumbers</li></ul> | b | True if no error happens | ✔ | |
| setallbrightness | d(bdu)s | <ul><li>Target pct</li><li>Smooth struct</li><li>Internal laptop's screen interface</li></ul> | b | True if no error happens | ✔ | |
| getgamma | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current display gamma temp | | ✔ |
| setgamma | ssi(buu) | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New gamma value</li><li>Smooth struct</li></ul> | b | True if no error happens | ✔ | ✔ |
| captureframes | si | <ul><li>video sysname(eg: Video0)</li><li>Number of frames</li></ul> | ad | Each frame's brightness (0-255) | ✔ | |
| iswebcamavailable | | | b | True if any webcam could be found | | |
| getdpms | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current dpms state | |✔|
| setdpms | ssi | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New dpms state</li></ul> | i | New setted dpms state | ✔ | ✔ |
| getdpms_timeouts | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | iii | Dpms timeouts values |  | ✔ |
| setdpms_timeouts | ssiii | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New dpms timeouts</li></ul> | iii | New dpms timeouts | ✔ | ✔ |
| getidletime | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current idle time in ms | | ✔ |

### Properties

| Prop | OUT | OUT values |
|-|:-:|-|
| version | s | <ul><li>Clightd version</li></ul> |

### Signals

| Sig | OUT | OUT values |
|-|:-:|-|
| WebcamChanged | ss | <ul><li>Webcam's devpath</li><li>Action string, as received from udev. Eg: "add", "remove"</li></ul> |


## Ddcutil support
Clightd uses [ddcutil](https://github.com/rockowitz/ddcutil) C api to set external monitor brightness and thus supporting desktop PCs too.  

> ddcutil is a program for querying and changing monitor settings, such as brightness and color levels.  
> ddcutil uses DDC/CI to communicate with monitors implementing MCCS (Monitor Control Command Set) over I2C.  

Its support is obviously optional, as it is unfortunately not widely available (eg: ubuntu/debian do not even package ddcutil C api).  
It is being pushed though (kde plasma is going to use it: https://phabricator.kde.org/D5381).  
Users wishing to use ddcutil features, should install a modules-load.d conf file. Use:

    # make install WITH_DDC=1

On archlinux this is automatically accomplished by PKGBUILD.

## Arch AUR packages
Clightd is available on AUR: https://aur.archlinux.org/packages/clightd-git/ .

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clightd/blob/master/COPYING) file for more informations.
