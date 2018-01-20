# Clightd

[![Build Status](https://travis-ci.org/FedeDP/Clightd.svg?branch=master)](https://travis-ci.org/FedeDP/Clightd)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/7563e6f8e83f4b1aa884c6032709e341)](https://www.codacy.com/app/FedeDP/Clightd?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=FedeDP/Clightd&amp;utm_campaign=Badge_Grade)

Clightd is a bus interface that lets you easily set screen brightness, gamma temperature and get ambient brightness through webcam frames capture.

## It currently needs:
* libsystemd >= 221 (systemd/sd-bus.h)
* libudev (libudev.h)

### Needed only if built with gamma support:
* libxrandr (X11/extensions/Xrandr.h)
* libx11 (X11/Xlib.h)

### Needed only if built with dpms support:
* libxext (X11/extensions/Xext.h)
* libx11 (X11/Xlib.h)

### Needed only if built with idle time support:
* libxss (X11/extensions/scrnsaver.h)
* libx11 (X11/Xlib.h)

### Needed only if built with frame captures support:
* linux-api-headers (linux/videodev2.h)

### Needed only if built with ddcutil support:
* ddcutil (ddcutil_c_api.h)

## Runtime deps:
* shared objects from build libraries
* polkit

## Build time switches:
* DISABLE_FRAME_CAPTURES=1 (to disable frame captures support)
* DISABLE_GAMMA=1 (to disable gamma support)
* DISABLE_DPMS=1 (to disable dpms support)
* DISABLE_IDLE=1 (to disable user idle time support)
* DISABLE_DDC=1 (to disable [ddcutil](https://github.com/FedeDP/Clightd/tree/ddcutil#ddcutil-support) support)

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
Brightness related bus interface methods make all use of libudev to write and read current values (no fopen or other things like that).  
All method calls use libudev to take correct device path, and fallback to first subsystem matching device if empty string is passed.  
Strict error checking tries to enforce no issue of any kind.  

Getgamma function supports 50-steps temperature values. It tries to fit temperature inside a 50 step (eg: it finds 5238, tries if 5200 or 5250 are fine too, and in case returns them. Otherwise, it returns 5238.)  

Clightd makes use of polkit for setgamma, setbrightness, setdpms and captureframes function. Only active sessions can call these methods.  

You may ask why did i developed this solution. The answer is quite simple: on linux there is no simple and unified way of changing screen brightness.  
So, i thought it could be a good idea to develop a bus service that can be used by every other program.  

My idea is that anyone can now implement something similar to [clight](https://github.com/FedeDP/Clight) without messing with videodev/libudev and code in general.  
A clight replacement, using clightd, can be something like (pseudo-code):

    $ ambient_br = busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight captureframes "si" "" 5
    $ avg_br_percent = compute_avg(ambient_br, 5)
    $ busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight setbrightness "sd" "" avg_br_percent

**Note that passing an empty/NULL string as first parameter will make clightd use first subsystem matching device it finds (through libudev).** It should be good to go in most cases.

## Bus interface
| Method | IN | IN values | OUT | OUT values | Polkit restricted | X only |
|-|:-:|-|:-:|-|:-:|-|
| getbrightness | s | <ul><li>Backlight kernel interface (eg: intel_backlight) or empty string</li></ul> | i | Interface's brightness | | |
| getmaxbrightness | s | <ul><li>Backlight kernel interface</li></ul> | i | Interface's max brightness | | |
| getactualbrightness | s | <ul><li>Backlight kernel interface</li></ul> | i | Interface's actual brightness | | |
| setbrightness | sd | <ul><li>Backlight kernel interface</li><li>New brightness as percentage of max value</li></ul>| i | Number of screens on which brightness has been changed | ✔ | |
| getgamma | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current display gamma temp | | ✔ |
| setgamma | ssi | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New gamma value</li></ul> | i | New setted gamma temp | ✔ | ✔ |
| captureframes | si | <ul><li>video sysname(eg: Video0)</li><li>Number of frames</li></ul> | ad | Each frame's brightness (0-255) | ✔ | |
| getdpms | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current dpms state | |✔|
| setdpms | ssi | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New dpms state</li></ul> | i | New setted dpms state | ✔ | ✔ |
| getdpms_timeouts | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | iii | Dpms timeouts values |  | ✔ |
| setdpms_timeouts | ssiii | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New dpms timeouts</li></ul> | iii | New dpms timeouts | ✔ | ✔ |
| getidletime | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current idle time in ms | | ✔ |

## Ddcutil support
Clightd uses [ddcutil](https://github.com/rockowitz/ddcutil) C api to set external monitor brightness.  

> ddcutil is a program for querying and changing monitor settings, such as brightness and color levels.  
> ddcutil uses DDC/CI to communicate with monitors implementing MCCS (Monitor Control Command Set) over I2C.  

Its support is obviously optional, as it is unfortunately not widely available (eg: ubuntu/debian do not even package ddcutil C api).  
It is being pushed though (kde plasma is going to use it: https://phabricator.kde.org/D5381).  
Users must manually create a file in /etc/modules-load.d/ to load i2c-dev module at boot, like this [one](https://github.com/FedeDP/Clightd/blob/ddcutil/Scripts/i2c_clightd.conf).  
On archlinux this is automatically accomplished by PKGBUILD.

## Arch AUR packages
Clightd is available on AUR: https://aur.archlinux.org/packages/clightd-git/ .

## Deb packages
Deb package for amd64 architecture is provided for each [release](https://github.com/FedeDP/Clightd/releases).  
Moreover, you can very easily build your own packages, if you wish to test latest Clightd code.  
You only need to issue:

    $ make deb

A deb file will be created in "Debian" folder, inside Clightd root.  
Please note that while i am using Debian testing at my job, i am developing clightd from archlinux.  
Thus, you can encounter some packaging issues. Please, report back.  

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clightd/blob/master/COPYING) file for more informations.
