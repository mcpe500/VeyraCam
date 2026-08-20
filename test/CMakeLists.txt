cmake_minimum_required(VERSION 3.20)
project(VeyraE2ETest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../native/core)

# --- mbedTLS (audited crypto; fixes C-1: no hand-rolled ciphers) ---
include(FetchContent)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(GEN_FILES OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2
    URL_HASH SHA256=8b54fb9bcf4d5a7078028e0520acddefb7900b3e66fec7f7175bb5b7d85ccdca
)
FetchContent_MakeAvailable(mbedtls)

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
target_link_libraries(e2e_streaming_test Threads::Threads mbedcrypto)
