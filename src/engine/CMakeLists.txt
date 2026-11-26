cmake_minimum_required(VERSION 3.20)

project(strafe_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(engine STATIC
    engine.cpp
    bindings/bindings.cpp
    input/controller.cpp
    input/desktop_input.cpp
    input/elite_input.cpp
    input/playstation_input.cpp
    input/raw_input.cpp
    output/keys.cpp
    output/output.cpp
    output/virtual_duo.cpp
    profiles/json.cpp
    profiles/profile.cpp
    profiles/rewasd_import.cpp
    remapping/remap_engine.cpp
    remapping/remapper.cpp
    util/text.cpp
)
target_compile_features(engine PUBLIC cxx_std_20)
target_include_directories(engine PUBLIC include PRIVATE .)
target_compile_definitions(engine PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
target_compile_options(engine PRIVATE /W4 /permissive- /MP /utf-8)
target_link_libraries(engine PRIVATE user32 xinput cfgmgr32 hid windowsapp)
set_property(TARGET engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE engine)
    target_compile_definitions(engine_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
    target_compile_options(engine_tests PRIVATE /W4 /permissive- /utf-8)
    target_include_directories(engine_tests PRIVATE .)
endif()
