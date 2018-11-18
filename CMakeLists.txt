cmake_minimum_required(VERSION 3.5)

project(clightd VERSION 2.4 LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
find_package(PkgConfig)

# Create program target
file(GLOB_RECURSE SOURCES src/*.c)
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
                           # Internal headers
                           "${CMAKE_CURRENT_SOURCE_DIR}/src"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/modules"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -D_GNU_SOURCE
    -DVERSION="${PROJECT_VERSION}"
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)

# Required dependencies
pkg_check_modules(REQ_LIBS REQUIRED libudev libmodule)
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
    option(DISABLE_${name}
           "Disable support for ${description} (defaults to use if found)"
           OFF)
    if(${DISABLE_${name}})
        message(STATUS "${name} support disabled: User choice")
    else()
        pkg_check_modules(${name}_LIBS ${modules})
        if(${${name}_LIBS_FOUND})
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
            message(STATUS "${name} support disabled: Not found")
        endif()
    endif()
endmacro()

optional_dep(GAMMA "x11;xrandr" "Gamma")
optional_dep(DPMS "x11;xext" "DPMS")
optional_dep(IDLE "x11;xscrnsaver" "IDLE")
optional_dep(DDC "ddcutil" "DDC")

# Convert ld flag list from list to space separated string.
string(REPLACE ";" " " COMBINED_LDFLAGS "${COMBINED_LDFLAGS}")

# Set the LDFLAGS target property
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    LINK_FLAGS "${COMBINED_LDFLAGS}"
)

# Installation of targets (must be before file configuration to work)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}")

# Configure files with install paths
set(SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Scripts")
set(DAEMON_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME}")

configure_file(${SCRIPT_DIR}/clightd.service clightd.service @ONLY)
configure_file(${SCRIPT_DIR}/org.clightd.clightd.service
               org.clightd.clightd.service
               @ONLY)
# Installation of files

pkg_get_variable(MODULE_LOAD_DIR systemd modulesloaddir)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/clightd.service
        DESTINATION /usr/lib/systemd/system)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.clightd.clightd.service
        DESTINATION /usr/share/dbus-1/system-services)
install(FILES ${SCRIPT_DIR}/org.clightd.clightd.policy
        DESTINATION /usr/share/polkit-1/actions)
if (WITH_DDC)
    install(FILES ${SCRIPT_DIR}/i2c_clightd.conf
            DESTINATION "${MODULE_LOAD_DIR}")
endif()
install(FILES ${SCRIPT_DIR}/org.clightd.clightd.conf
        DESTINATION /etc/dbus-1/system.d/)
install(FILES COPYING
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME}")
