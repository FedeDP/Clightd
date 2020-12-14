# Stolen from: https://github.com/giucam/orbital/blob/512c1b3b20a32cf67ba326e8acce3c1c32e11fa2/cmake/Wayland.cmake, thanks!
function(WAYLAND_ADD_PROTOCOL_CLIENT protocol)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

    get_filename_component(proto_name ${protocol} NAME_WLE)
    get_filename_component(proto_fullpath ${protocol} ABSOLUTE)
    message(STATUS "Enabled '${proto_name}' wayland client protocol")
        
    set(_client_header "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}-client-protocol.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}-protocol.c")
            
    add_custom_command(
        OUTPUT "${_client_header}" "${_code}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header < ${proto_fullpath} > ${_client_header}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code < ${proto_fullpath} > ${_code}
        DEPENDS ${proto_fullpath} VERBATIM
    )

    target_sources(${PROJECT_NAME} PRIVATE ${_code})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()
