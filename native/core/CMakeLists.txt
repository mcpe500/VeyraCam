cmake_minimum_required(VERSION 3.15)
project(veyra_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

include_directories(include)

set(CORE_SOURCES
    src/protocol.cpp
    src/packetizer.cpp
    src/ring_buffer.cpp
    src/jitter_buffer.cpp
    src/transport_manager.cpp
    src/crypto.cpp
    src/telemetry.cpp
    src/session.cpp
)

add_library(veyra_core STATIC ${CORE_SOURCES})

target_include_directories(veyra_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(veyra_core PUBLIC mbedcrypto)

# Core Unit Tests
enable_testing()
add_executable(test_veyra_core
    tests/test_main.cpp
)

target_link_libraries(test_veyra_core PRIVATE veyra_core)
add_test(NAME CoreTests COMMAND test_veyra_core)
