cmake_minimum_required(VERSION 3.24)

project(KSGE
    VERSION 0.1.0
    DESCRIPTION "Kizuri Studio Game Engine"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

set(KSGE_FLECS_TARGET $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>)

add_library(ksge_engine STATIC
    engine/assets/gltf.cpp
    engine/assets/registry.cpp
    engine/assets/texture.cpp
    engine/core/world.cpp
    engine/graphics/device.cpp
    engine/graphics/mesh_upload.cpp
    engine/graphics/postprocess.cpp
    engine/graphics/renderer.cpp
    engine/graphics/shader_compiler.cpp
    engine/platform/input.cpp
    engine/platform/window.cpp
    engine/scene/camera.cpp
    engine/scene/camera_service.cpp
)
target_include_directories(ksge_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ksge_engine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(ksge_engine PRIVATE $<$<CONFIG:Release>:/O2> /W4 /WX /EHsc)
target_link_libraries(ksge_engine PUBLIC
    glfw
    ${KSGE_FLECS_TARGET}
    Microsoft::DirectXMath
    fastgltf::fastgltf
    d3d11
    dxgi
    d3dcompiler
    windowscodecs
    ole32
)

add_executable(ksge_editor editor/main.cpp)
target_compile_options(ksge_editor PRIVATE $<$<CONFIG:Release>:/O2> /W4 /WX /EHsc)
target_link_libraries(ksge_editor PRIVATE ksge_engine)

enable_testing()
add_executable(ksge_tests
    tests/asset_tests.cpp
    tests/graphics_tests.cpp
    tests/run_tests.cpp
)
target_compile_options(ksge_tests PRIVATE $<$<CONFIG:Release>:/O2> /W4 /WX /EHsc)
target_link_libraries(ksge_tests PRIVATE ksge_engine)
add_test(NAME ksge_tests COMMAND ksge_tests)