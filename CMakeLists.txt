cmake_minimum_required(VERSION 3.14)

project(clightd VERSION 5.8 LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
find_package(PkgConfig)

execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Create program target
file(GLOB SOURCES src/*.c src/utils/*.c src/modules/*.c src/modules/sensors/*.c)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
                           # Internal headers
                           "${CMAKE_CURRENT_SOURCE_DIR}/src"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/modules"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -D_GNU_SOURCE
    -DVERSION="${PROJECT_VERSION}-${GIT_HASH}"
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)

# Required dependencies
pkg_check_modules(REQ_LIBS REQUIRED libudev libmodule>=5.0.0 libjpeg)
pkg_check_modules(POLKIT REQUIRED polkit-gobject-1)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_search_module(LOGIN_LIBS REQUIRED libelogind libsystemd>=221)
target_link_libraries(${PROJECT_NAME}
                      m
                      ${REQ_LIBS_LIBRARIES}
                      ${LOGIN_LIBS_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PRIVATE
                           "${REQ_LIBS_INCLUDE_DIRS}"
                           "${LOGIN_LIBS_INCLUDE_DIRS}"
)
list(APPEND COMBINED_LDFLAGS ${REQ_LIBS_LDFLAGS})
list(APPEND COMBINED_LDFLAGS ${LOGIN_LIBS_LDFLAGS})

# Optional dependencies

# Needed to eventually build wayland protocols
add_subdirectory(protocol)

# Helper macro for dealing correctly with optional pkg-config dependencies.
# There are a number of issues when using pkg-config with cmake (as compared to
# using the native dependency handling in CMake).
macro(optional_dep name modules description)
    option(ENABLE_${name}
           "Enable support for ${description} (defaults to not use it)"
           OFF)
    if(${ENABLE_${name}})
        pkg_check_modules(${name}_LIBS REQUIRED ${modules})
        message(STATUS "${name} support enabled")
        target_compile_definitions(${PROJECT_NAME} PRIVATE ${name}_PRESENT)
        # We can't use target_link_libraries, it will not proper handle
        # non-standard library paths, since pkg-config returns -Lpath -llib
        # instead of -l/path/lib.
        list(APPEND COMBINED_LDFLAGS ${${name}_LIBS_LDFLAGS})
        # The actual libraries need to be listed at the end of the link command,
        # so this is also needed.
        target_link_libraries(${PROJECT_NAME} ${${name}_LIBS_LIBRARIES})
        target_include_directories(${PROJECT_NAME}
                                   PRIVATE
                                   ${${name}_LIBS_INCLUDE_DIRS})
        set(WITH_${name} 1)
        
        # Check if optional parameters for wayland protocols were passed
        set(extra_macro_args ${ARGN})
        list(LENGTH extra_macro_args num_extra_args)
        if (${num_extra_args} GREATER 0)
            # Generate protocol source and header files
            # and add them to target_sources + target_include_directories(
            list(GET extra_macro_args 0 wl_proto)
            WAYLAND_ADD_PROTOCOL_CLIENT(${wl_proto})
            
            # Add various plugins
            list(GET extra_macro_args 1 src_proto)
            file(GLOB EXTRA_SRCS ${src_proto}/*)
            target_sources(${PROJECT_NAME} PRIVATE ${EXTRA_SRCS})
        endif()
    else()
        message(STATUS "${name} support disabled")
    endif()
endmacro()

optional_dep(GAMMA "x11;xrandr;libdrm;wayland-client" "Gamma correction" protocol/wlr-gamma-control-unstable-v1.xml src/modules/gamma_plugins)
optional_dep(DPMS "x11;xext;libdrm;wayland-client" "DPMS" protocol/org_kde_kwin_dpms.xml src/modules/dpms_plugins)
optional_dep(SCREEN "x11" "screen emitted brightness" protocol/wlr-screencopy-unstable-v1.xml src/modules/screen_plugins)
optional_dep(SCREEN "x11" "screen emitted brightness" protocol/wlr-output-management-unstable-v1.xml src/modules/screen_plugins)
optional_dep(DDC "ddcutil>=0.9.5" "external monitor backlight")
optional_dep(YOCTOLIGHT "libusb-1.0" "Yoctolight usb als devices support")
optional_dep(PIPEWIRE "libpipewire-0.3" "Enable pipewire camera sensor support")

# Convert ld flag list from list to space separated string.
string(REPLACE ";" " " COMBINED_LDFLAGS "${COMBINED_LDFLAGS}")

# Set the LDFLAGS target property
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "${COMBINED_LDFLAGS}"
)

# Installation of targets (must be before file configuration to work)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_LIBEXECDIR}")

set(SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Scripts")

#### Installation of files ####

# Only install systemd service in systemd environment
pkg_check_modules(SYSTEMD_BASE systemd)
if(SYSTEMD_BASE_FOUND)
    # Use polkitd.service on ubuntu 16.04
    if(EXISTS /lib/systemd/system/polkitd.service)
        set(POLKIT_NAME "polkitd")
    else()
        set(POLKIT_NAME "polkit")
    endif()
    
    if(WITH_DDC)
        set(AFTER "After=systemd-modules-load.service")
    endif()
    
    # Properly configure clightd systemd service to use correct dep on polkit.service
    configure_file(${SCRIPT_DIR}/clightd.service clightd.service @ONLY)
    
    # This can be overridden by cmdline
    if(NOT SYSTEMD_SERVICE_DIR)
        # Fetch it from systemd
        pkg_get_variable(SYSTEMD_SERVICE_DIR systemd systemdsystemunitdir)
    endif()

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/clightd.service
        DESTINATION ${SYSTEMD_SERVICE_DIR})

    set(SYSTEMD_SERVICE "SystemdService=clightd.service")
endif()

# Install dbus service
configure_file(${SCRIPT_DIR}/org.clightd.clightd.service
               org.clightd.clightd.service
               @ONLY)
pkg_get_variable(SYSTEM_BUS_DIR dbus-1 system_bus_services_dir)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.clightd.clightd.service
        DESTINATION ${SYSTEM_BUS_DIR})

# Install polkit policy
pkg_get_variable(POLKIT_ACTION_DIR polkit-gobject-1 actiondir)
install(FILES ${SCRIPT_DIR}/org.clightd.clightd.policy
        DESTINATION ${POLKIT_ACTION_DIR})
        
# When using DDC, try to install an i2c kernel module loading script
if(WITH_DDC)
    # Users can provide a MODULE_LOAD_DIR
    if(NOT MODULE_LOAD_DIR)
        if(SYSTEMD_BASE_FOUND)
            # Fetch it from systemd
            pkg_get_variable(MODULE_LOAD_DIR systemd modulesloaddir)
        else()
            # No luck.
            message(WARNING "Automatic loading of i2c module disabled.")
        endif()
    endif()
    if(MODULE_LOAD_DIR)
        install(FILES ${SCRIPT_DIR}/i2c_clightd.conf
                DESTINATION "${MODULE_LOAD_DIR}")
    endif()
endif()

# Install dbus conf
if(NOT DBUS_CONFIG_DIR)
    pkg_get_variable(DBUS_SYSTEM_CONF_DIR dbus-1 sysconfdir)
    set(DBUS_CONFIG_DIR "${DBUS_SYSTEM_CONF_DIR}/dbus-1/system.d/" CACHE PATH "dbus config directory")
    message(STATUS "Using default value for dbus config dir: ${DBUS_CONFIG_DIR}")
endif()
install(FILES ${SCRIPT_DIR}/org.clightd.clightd.conf
        DESTINATION ${DBUS_CONFIG_DIR})

#
# Packaging support
#
SET(CPACK_SET_DESTDIR "on")
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

#
#  Metadata common to all packaging systems
#
set(CPACK_PACKAGE_CONTACT "Federico Di Pierro <nierro92@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION "Clightd offers a bus interface that lets you easily set screen brightness, gamma temperature and get ambient brightness through webcam frames capture or ALS devices.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Backlight-related bus API.")

#
#  RPM Specific configuration
#
set(CPACK_RPM_PACKAGE_LICENSE "GPL")
set(CPACK_RPM_PACKAGE_URL "https://github.com/FedeDP/Clightd")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${DBUS_SYSTEM_CONF_DIR}/dbus-1" "${DBUS_CONFIG_DIR}" "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_LIBDIR}" "${SYSTEM_BUS_DIR}" "${POLKIT_ACTION_DIR}")
set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
set(CPACK_RPM_PACKAGE_REQUIRES "libmodule >= 5.0.0")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

#
#  DEB Specific configuration
#
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/FedeDP/Clightd")
set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libmodule (>=5.0.0)")

# Common deps
if(WITH_DDC)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, ddcutil (>=0.9.5)")
    set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES} ddcutil >= 0.9.5")
endif()

include(CPack)
