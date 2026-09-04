cmake_minimum_required(VERSION 3.24)

project(KSGE
    VERSION 0.1.0
    DESCRIPTION "Kizuri Studio Game Engine"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ksge_editor editor/main.cpp)
target_compile_options(ksge_editor PRIVATE $<$<CONFIG:Release>:/O2> /W4 /EHsc)

add_executable(ksge_exporter exporter/main.cpp)
target_compile_options(ksge_exporter PRIVATE $<$<CONFIG:Release>:/O2> /W4 /EHsc)

find_package(glfw3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

set(KSCE_FLECS_TARGET $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>)

target_link_libraries(ksge_editor PRIVATE glfw ${KSCE_FLECS_TARGET} Microsoft::DirectXMath fastgltf::fastgltf)
target_link_libraries(ksge_exporter PRIVATE Microsoft::DirectXMath)