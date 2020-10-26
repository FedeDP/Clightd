cmake_minimum_required(VERSION 3.5)

project(clightd VERSION 4.3 LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
find_package(PkgConfig)

# Only needed if building wayland protocols (ie: when ENABLE_GAMMA or ENABLE_DPMS are true)
find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)

# wayland_add_protocol_client(outfiles inputfile basename)
# Stolen from: https://github.com/giucam/orbital/blob/512c1b3b20a32cf67ba326e8acce3c1c32e11fa2/cmake/Wayland.cmake, thanks!
function(WAYLAND_ADD_PROTOCOL_CLIENT _sources _protocol _basename)
    if(NOT WAYLAND_SCANNER_EXECUTABLE)
        message(FATAL "The wayland-scanner executable has not been found on your system. You must install it.")
    endif()

    get_filename_component(_infile ${_protocol} ABSOLUTE)
    set(_client_header "${CMAKE_CURRENT_BINARY_DIR}/${_basename}-client-protocol.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/${_basename}-protocol.c")

    add_custom_command(OUTPUT "${_client_header}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header < ${_infile} > ${_client_header}
        DEPENDS ${_infile} VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code < ${_infile} > ${_code}
        DEPENDS ${_infile} VERBATIM)

    list(APPEND ${_sources} "${_client_header}" "${_code}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()

# Create program target
file(GLOB_RECURSE SOURCES src/*.c)

# Build wayland protocols if needed
if(ENABLE_GAMMA)
    WAYLAND_ADD_PROTOCOL_CLIENT(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/protocol/wlr-gamma-control-unstable-v1.xml" wlr-gamma-control-unstable-v1)
endif()
if(ENABLE_DPMS)
    WAYLAND_ADD_PROTOCOL_CLIENT(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/protocol/dpms.xml" dpms)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
                           # Internal headers
                           "${CMAKE_CURRENT_SOURCE_DIR}/src"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/modules"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/dpms_plugins"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -D_GNU_SOURCE
    -DVERSION="${PROJECT_VERSION}"
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)

# Required dependencies
pkg_check_modules(REQ_LIBS REQUIRED libudev libmodule>=5.0.0 libjpeg)
pkg_check_modules(POLKIT REQUIRED polkit-gobject-1)
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
    else()
        message(STATUS "${name} support disabled")
    endif()
endmacro()

optional_dep(GAMMA "x11;xrandr;libdrm;wayland-client" "Gamma correction")
optional_dep(DPMS "x11;xext;libdrm;wayland-client" "DPMS")
optional_dep(SCREEN "x11" "screen emitted brightness")
optional_dep(DDC "ddcutil>=0.9.5" "external monitor backlight")
optional_dep(YOCTOLIGHT "libusb-1.0" "Yoctolight usb als devices support")

# Convert ld flag list from list to space separated string.
string(REPLACE ";" " " COMBINED_LDFLAGS "${COMBINED_LDFLAGS}")

# Set the LDFLAGS target property
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "${COMBINED_LDFLAGS}"
)

# Installation of targets (must be before file configuration to work)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}")

# Configure files with install paths
set(SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Scripts")
set(DAEMON_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME}")

configure_file(${SCRIPT_DIR}/org.clightd.clightd.service
               org.clightd.clightd.service
               @ONLY)

# Installation of files
pkg_get_variable(SYSTEM_BUS_DIR dbus-1 system_bus_services_dir)
pkg_get_variable(POLKIT_ACTION_DIR polkit-gobject-1 actiondir)

# Only install systemd service in systemd environment
pkg_check_modules(SYSTEMD libsystemd)
if(SYSTEMD_FOUND)
    # Use polkitd.service on ubuntu 16.04
    if(EXISTS /lib/systemd/system/polkitd.service)
        set(POLKIT_NAME "polkitd")
    else()
        set(POLKIT_NAME "polkit")
    endif()
    # Properly configure clightd systemd service to use correct dep on polkit.service
    configure_file(${SCRIPT_DIR}/clightd.service clightd.service @ONLY)
    
    # This can be overridden bt cmdline
    set(SYSTEMD_SERVICE_DIR ${CMAKE_INSTALL_LIBDIR}/systemd/system CACHE PATH
        "Systemd services directory")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/clightd.service
        DESTINATION ${SYSTEMD_SERVICE_DIR})
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.clightd.clightd.service
        DESTINATION ${SYSTEM_BUS_DIR})
install(FILES ${SCRIPT_DIR}/org.clightd.clightd.policy
        DESTINATION ${POLKIT_ACTION_DIR})
if(WITH_DDC)
    pkg_get_variable(MODULE_LOAD_DIR systemd modulesloaddir)
    if(MODULE_LOAD_DIR)
        install(FILES ${SCRIPT_DIR}/i2c_clightd.conf
                DESTINATION "${MODULE_LOAD_DIR}")
    endif()
endif()
install(FILES ${SCRIPT_DIR}/org.clightd.clightd.conf
        DESTINATION /etc/dbus-1/system.d/)
        
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
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/etc/dbus-1" "/etc/dbus-1/system.d" "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_LIBDIR}" "${SYSTEM_BUS_DIR}" "${POLKIT_ACTION_DIR}")
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
