include(FetchContent)

function(WAYLAND_ADD_PROTOCOL_CLIENT protocol)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

    get_filename_component(proto_name ${protocol} NAME_WLE)
    FetchContent_Declare(${proto_name} URL ${protocol} DOWNLOAD_NO_EXTRACT TRUE)
    FetchContent_Populate(${proto_name})
    
    message(STATUS "Enabled '${proto_name}' wayland client protocol")
        
    set(_client_header "${${proto_name}_BINARY_DIR}/${proto_name}-client-protocol.h")
    set(_code "${${proto_name}_BINARY_DIR}/${proto_name}-protocol.c")
    # set(_client_header "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}-client-protocol.h")
    # set(_code "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}-protocol.c")
    set(proto_fullpath "${${proto_name}_SOURCE_DIR}/${proto_name}.xml")
            
    add_custom_command(
        OUTPUT "${_client_header}" "${_code}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header < ${proto_fullpath} > ${_client_header}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code < ${proto_fullpath} > ${_code}
        DEPENDS ${proto_fullpath} VERBATIM
    )

    target_sources(${PROJECT_NAME} PRIVATE ${_code})
    target_include_directories(${PROJECT_NAME} PRIVATE ${${proto_name}_BINARY_DIR})
endfunction()

