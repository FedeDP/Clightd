# Clightd

[![Build Status](https://travis-ci.org/FedeDP/Clightd.svg?branch=master)](https://travis-ci.org/FedeDP/Clightd)

Clightd is a bus interface that let you easily set screen brightness, gamma temperature and get ambient brightness through webcam frames capture.

## It currently needs:
* libsystemd >= 221 (systemd/sd-bus.h)
* libudev (libudev.h)

### Needed only if built with gamma support:
* libxrandr (X11/extensions/Xrandr.h)
* libx11 (X11/Xlib.h)

### Needed only if built with frame captures support:
* linux-api-headers (linux/videodev2.h)

## Build time switches:
* DISABLE_FRAME_CAPTURES=1 (to disable frame captures support)
* DISABLE_GAMMA=1 (to disable gamma support)

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
If no syspath is passed as parameter to method calls, it uses first subsystem matching device that it finds through libudev.  
Strict error checking tries to enforce no issue of any kind.  

Getgamma function supports 50-steps temperature values. It tries to fit temperature inside a 50 step (eg: it finds 5238, tries if 5200 or 5250 are fine too, and in case returns them. Otherwise, it returns 5238.)

You may ask why did i developed this solution. The answer is quite simple: on linux there is no simple and unified way of changing screen brightness.  
So, i thought it could be a good idea to develop a bus service that can be used by every other program.  

My idea is that anyone can now implement something similar to clight without messing with videodev or libjpeg.  
A clight replacement, using clightd, can be something like (pseudo-code):

    $ max_br = busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight getmaxbrightness s ""
    $ ambient_br = busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight captureframes si "" 5
    $ new_br = ambient_br * max_br
    $ busctl call org.clightd.backlight /org/clightd/backlight org.clightd.backlight setbrightness si "" new_br

Note that passing an empty string as first parameter will make clightd use first subsystem matching device it finds (through libudev). It should be good to go in most cases.

## Bus interface methods
* *getbrightness* -> takes a backlight kernel interface (eg: intel_backlight) or nothing to just use first backlight kernel interface that libudev finds.
Returns current brightness value (int).
* *getmaxbrightness* -> takes a backlight kernel interface (as above). Returns max supported brightness value for that interface (int).
* *setbrightness* -> takes a backlight kernel interface and a new value. Set the brightness value on that interface and returns new brightness value (int).
Note that new brightness value is checked to be between 0 and max_brightness.
* *getactualbrightness* -> takes a backlight kernel interface. Returns actual brightness for that interface (int).

### If built with gamma support:
* *getgamma* -> returns current display temperature (int).
* *setgamma* -> takes a temperature value (int, between 1000 and 10000) and set display temperature. Returns newly setted display temperature (int).

### If built with frame captures support:
* *captureframes* -> takes a video sysname (eg: video0) and a number of frames to be captured (int, between 1 and 20). Returns average frames brightness, between 0.0 and 1.0 (double).


## Arch AUR packages
In [Arch](https://github.com/FedeDP/Clightd/tree/master/Arch) folder you can find a PKGBUILD.  
I will upload clightd to aur as soon as i feel it is ready to be used.  

## Deb packages
Deb package for amd64 architecture is provided. You can find it inside [Debian](https://github.com/FedeDP/Clightd/tree/master/Debian) folder.  
Moreover, you can very easily build your own packages, if aforementioned package happens to be outdated.  
You only need to issue:

    $ make deb

A deb file will be created in [Debian](https://github.com/FedeDP/Clightd/tree/master/Debian) folder.  
Please note that while i am using Debian testing at my job, i am developing clightd from archlinux.  
Thus, you can encounter some packaging issues. Please, report back.  

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clightd/blob/master/COPYING) file for more informations.
