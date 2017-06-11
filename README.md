# Clightd

[![Build Status](https://travis-ci.org/FedeDP/Clightd.svg?branch=master)](https://travis-ci.org/FedeDP/Clightd)

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

## Runtime deps:
* shared objects from build libraries
* polkit

## Build time switches:
* DISABLE_FRAME_CAPTURES=1 (to disable frame captures support)
* DISABLE_GAMMA=1 (to disable gamma support)
* DISABLE_DPMS=1 (to disable dpms support)
* DISABLE_IDLE=1 (to disable user idle time support)

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

    $ max_br = busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight getmaxbrightness s ""
    $ ambient_br = busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight captureframes si "" 5
    $ new_br = ambient_br * max_br
    $ busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight setbrightness si "" new_br

Note that passing an empty/NULL string as first parameter will make clightd use first subsystem matching device it finds (through libudev).* It should be good to go in most cases.

## Bus interface
| Method | IN | IN values | OUT | OUT values | Polkit restricted |
|-|:-:|-|:-:|-|:-:|
| getbrightness | s | <ul><li>Backlight kernel interface (eg: intel_backlight) or empty string</li></ul> | i | Interface's brightness | |
| getmaxbrightness | s | <ul><li>Backlight kernel interface</li></ul> | i | Interface's max brightness | |
| getactualbrightness | s | <ul><li>Backlight kernel interface</li></ul> | i | Interface's actual brightness | |
| setbrightness | si | <ul><li>Backlight kernel interface</li><li>New brightness value</li></ul>| i | New setted brightness |✔|
| getgamma | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current display gamma temp | |
| setgamma | ssi | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New gamma value</li></ul> | i | New setted gamma temp |✔|
| captureframes | si | <ul><li>video sysname(eg: Video0)</li><li>Number of frames</li></ul> | d | Average frames brightness, between 0.0 and 1.0 | ✔ |
| getdpms | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current dpms state | |
| setdpms | ssi | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New dpms state</li></ul> | i | New setted dpms state | ✔ |
| getdpms_timeouts | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | iii | Dpms timeouts values |  |
| setdpms_timeouts | ssiii | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li><li>New dpms timeouts</li></ul> | iii | New dpms timeouts |  ✔ |
| getidletime | ss | <ul><li>env DISPLAY</li><li>env XAUTHORITY</li></ul> | i | Current idle time in ms | |

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
