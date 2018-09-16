BINDIR = /usr/lib/clightd
BINNAME = clightd
BUSCONFDIR = /etc/dbus-1/system.d/
BUSCONFNAME = org.clightd.backlight.conf
BUSSERVICEDIR = /usr/share/dbus-1/system-services/
BUSSERVICENAME = org.clightd.backlight.service
SYSTEMDSERVICE = clightd.service
SYSTEMDDIR = /usr/lib/systemd/system
POLKITPOLICYNAME = org.clightd.backlight.policy
POLKITPOLICYDIR = /usr/share/polkit-1/actions
MODULESLOADDDC = i2c_clightd.conf
MODULESLOADDIR = /usr/lib/modules-load.d
EXTRADIR = Scripts
LICENSEDIR = /usr/share/licenses/clightd
RM = rm -f
RMDIR = rm -rf
INSTALL = install -p
INSTALL_PROGRAM = $(INSTALL) -m755
INSTALL_DATA = $(INSTALL) -m644
INSTALL_DIR = $(INSTALL) -d
SRCDIR = src/
LIBS = -lm $(shell pkg-config --libs libudev)
CFLAGS = $(shell pkg-config --cflags libudev) -D_GNU_SOURCE -std=c99

ifeq (,$(findstring $(MAKECMDGOALS),"clean install uninstall"))

ifeq ("$(shell pkg-config --exists libelogind && echo yes)", "yes")
LIBS+=$(shell pkg-config --libs libelogind)
CFLAGS+=$(shell pkg-config --cflags libelogind)
else
ifeq ("$(shell pkg-config --atleast-version=221 systemd && echo yes)", "yes")
LIBS+=$(shell pkg-config --libs libsystemd)
CFLAGS+=$(shell pkg-config --cflags libsystemd)
else
$(error systemd minimum required version 221.)
endif
endif

ifneq ("$(DISABLE_GAMMA)","1")
GAMMA=$(shell pkg-config --silence-errors --libs x11 xrandr)
ifneq ("$(GAMMA)","")
CFLAGS+=-DGAMMA_PRESENT $(shell pkg-config --cflags x11 xrandr)
$(info Gamma support enabled.)
else
$(info Gamma support disabled.)
endif
else
$(info Gamma support disabled.)
endif

ifneq ("$(DISABLE_DPMS)","1")
DPMS=$(shell pkg-config --silence-errors --libs x11 xext)
ifneq ("$(DPMS)","")
CFLAGS+=-DDPMS_PRESENT $(shell pkg-config --cflags x11 xext)
$(info DPMS support enabled.)
else
$(info DPMS support disabled.)
endif
else
$(info DPMS support disabled.)
endif

ifneq ("$(DISABLE_IDLE)","1")
IDLE=$(shell pkg-config --silence-errors --libs x11 xscrnsaver)
ifneq ("$(IDLE)","")
CFLAGS+=-DIDLE_PRESENT $(shell pkg-config --cflags x11 xscrnsaver)
$(info idle support enabled.)
else
$(info idle support disabled.)
endif
else
$(info idle support disabled.)
endif

ifneq ("$(DISABLE_DDC)","1")
DDC=$(shell pkg-config --silence-errors --libs ddcutil)
ifneq ("$(DDC)","")
CFLAGS+=-DUSE_DDC $(shell pkg-config --cflags ddcutil)
$(info DDCutil support enabled.)
else
$(info DDCutil support disabled.)
endif
else
$(info DDCutil support disabled.)
endif

LIBS+=$(GAMMA) $(DPMS) $(IDLE) $(DDC)

endif

CLIGHTD_VERSION = $(shell git describe --abbrev=0 --always --tags)
CFLAGS+=-DVERSION=\"$(CLIGHTD_VERSION)\"

all: clightd clean

debug: clightd-debug clean

objects:
	@cd $(SRCDIR); $(CC) -c *.c $(CFLAGS)

objects-debug:
	@cd $(SRCDIR); $(CC) -c *.c -Wall $(CFLAGS) -Wshadow -Wtype-limits -Wstrict-overflow -fno-strict-aliasing -Wformat -Wformat-security -g

clightd: objects
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clightd-debug: objects-debug
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clean:
	@cd $(SRCDIR); $(RM) *.o

install:
	$(info installing bin.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BINDIR)"
	@$(INSTALL_PROGRAM) $(BINNAME) "$(DESTDIR)$(BINDIR)"

	$(info installing dbus conf file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BUSCONFDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(BUSCONFNAME) "$(DESTDIR)$(BUSCONFDIR)"

	$(info installing dbus service file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BUSSERVICEDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(BUSSERVICENAME) "$(DESTDIR)$(BUSSERVICEDIR)"
	
	$(info installing systemd service file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(SYSTEMDDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(SYSTEMDSERVICE) "$(DESTDIR)$(SYSTEMDDIR)"
	
	$(info installing polkit policy file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(POLKITPOLICYDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(POLKITPOLICYNAME) "$(DESTDIR)$(POLKITPOLICYDIR)"
	
	$(info installing license file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(LICENSEDIR)"
	@$(INSTALL_DATA) COPYING "$(DESTDIR)$(LICENSEDIR)"
	
ifeq ("$(WITH_DDC)","1")
	$(info installing ddc module load file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(MODULESLOADDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(MODULESLOADDDC) "$(DESTDIR)$(MODULESLOADDIR)"
endif

uninstall:
	$(info uninstalling bin.)
	@$(RM) "$(DESTDIR)$(BINDIR)/$(BINNAME)"

	$(info uninstalling dbus conf file.)
	@$(RM) "$(DESTDIR)$(BUSCONFDIR)/$(BUSCONFNAME)"

	$(info uninstalling dbus service file.)
	@$(RM) "$(DESTDIR)$(BUSSERVICEDIR)/$(BUSSERVICENAME)"
	
	$(info uninstalling systemd service file.)
	@$(RM) "$(DESTDIR)$(SYSTEMDDIR)/$(SYSTEMDSERVICE)"
	
	$(info uninstalling polkit policy file.)
	@$(RM) "$(DESTDIR)$(POLKITPOLICYDIR)/$(POLKITPOLICYNAME)"
	
	$(info uninstalling license file.)
	@$(RMDIR) "$(DESTDIR)$(LICENSEDIR)"
	
	$(info uninstalling ddc module load file (if present).)
	@$(RM) "$(DESTDIR)$(MODULESLOADDIR)/$(MODULESLOADDDC)"
