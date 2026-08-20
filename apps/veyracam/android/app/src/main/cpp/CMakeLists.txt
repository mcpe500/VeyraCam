cmake_minimum_required(VERSION 3.18.0)
project(veyra_jni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VEYRA_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../native/core)

include_directories(
    ${VEYRA_CORE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(veyra_jni SHARED
    veyra_jni_bridge.cpp
    ${VEYRA_CORE_DIR}/src/protocol.cpp
    ${VEYRA_CORE_DIR}/src/packetizer.cpp
    ${VEYRA_CORE_DIR}/src/ring_buffer.cpp
    ${VEYRA_CORE_DIR}/src/jitter_buffer.cpp
    ${VEYRA_CORE_DIR}/src/transport_manager.cpp
    ${VEYRA_CORE_DIR}/src/session.cpp
    ${VEYRA_CORE_DIR}/src/crypto.cpp
    ${VEYRA_CORE_DIR}/src/telemetry.cpp
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(veyra_jni
    ${log-lib}
    ${android-lib}
)
