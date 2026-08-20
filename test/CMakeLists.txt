cmake_minimum_required(VERSION 3.20)
project(VeyraE2ETest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../native/core)

include_directories(
    ${CORE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(e2e_streaming_test
    e2e_streaming_test.cpp
    ${CORE_DIR}/src/protocol.cpp
    ${CORE_DIR}/src/packetizer.cpp
    ${CORE_DIR}/src/ring_buffer.cpp
    ${CORE_DIR}/src/jitter_buffer.cpp
    ${CORE_DIR}/src/transport_manager.cpp
    ${CORE_DIR}/src/session.cpp
    ${CORE_DIR}/src/crypto.cpp
    ${CORE_DIR}/src/telemetry.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(e2e_streaming_test Threads::Threads)
