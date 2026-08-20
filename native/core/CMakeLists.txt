cmake_minimum_required(VERSION 3.15)
project(veyra_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

# Core Unit Tests
enable_testing()
add_executable(test_veyra_core
    tests/test_main.cpp
)

target_link_libraries(test_veyra_core PRIVATE veyra_core)
add_test(NAME CoreTests COMMAND test_veyra_core)
