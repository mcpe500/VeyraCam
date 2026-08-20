cmake_minimum_required(VERSION 3.20)
project(VeyraWindows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../core)

include_directories(
    ${CORE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/virtual_camera/mf
    ${CMAKE_CURRENT_SOURCE_DIR}/virtual_camera/dshow
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
)

set(COMMON_CORE_SOURCES
    ${CORE_DIR}/src/protocol.cpp
    ${CORE_DIR}/src/packetizer.cpp
    ${CORE_DIR}/src/ring_buffer.cpp
    ${CORE_DIR}/src/jitter_buffer.cpp
    ${CORE_DIR}/src/transport_manager.cpp
    ${CORE_DIR}/src/session.cpp
    ${CORE_DIR}/src/crypto.cpp
    ${CORE_DIR}/src/telemetry.cpp
)

set(WINDOWS_SUBSYSTEM_SOURCES
    decoder/mf_decoder.cpp
    renderer/d3d11_renderer.cpp
    network/iocp_udp_server.cpp
    network/tcp_control_server.cpp
    network/win_rfcomm.cpp
    network/win_usb.cpp
    virtual_camera/mf/mf_virtual_camera.cpp
    virtual_camera/mf/virtual_camera_source.cpp
    virtual_camera/dshow/dshow_filter.cpp
    virtual_camera/dshow/dshow_pin.cpp
    virtual_camera/dshow/dshow_registration.cpp
    core/tray_icon.cpp
    core/service_manager.cpp
)

# 1. Standalone Tray Executable: VeyraCore
add_executable(VeyraCore
    core/main.cpp
    ${WINDOWS_SUBSYSTEM_SOURCES}
    ${COMMON_CORE_SOURCES}
)

# 2. Shared Library for Flutter Dart FFI: veyra_core_api
add_library(veyra_core_api SHARED
    ipc/veyra_ffi_export.cpp
    ${WINDOWS_SUBSYSTEM_SOURCES}
    ${COMMON_CORE_SOURCES}
)

if (WIN32)
    target_link_libraries(VeyraCore
        mfplat
        mfuuid
        wmcodecdspuuid
        d3d11
        dxgi
        ws2_32
        strmiids
        shell32
        user32
        ole32
        oleaut32
    )

    target_link_libraries(veyra_core_api
        mfplat
        mfuuid
        wmcodecdspuuid
        d3d11
        dxgi
        ws2_32
        strmiids
        shell32
        user32
        ole32
        oleaut32
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(VeyraCore Threads::Threads)
    target_link_libraries(veyra_core_api Threads::Threads)
endif()
