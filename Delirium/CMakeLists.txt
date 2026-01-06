cmake_minimum_required(VERSION 3.15)
project(DeliriumEngine VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g)
    else()
        add_compile_options(-O3)
    endif()
endif()

# Find Dependencies
find_package(SDL2 REQUIRED)
message(STATUS "SDL2 found: ${SDL2_LIBRARIES}")

find_package(OpenGL REQUIRED)
message(STATUS "OpenGL found: ${OPENGL_LIBRARIES}")

find_package(GLEW REQUIRED)
message(STATUS "GLEW found: ${GLEW_LIBRARIES}")

# Include Directories
include_directories(
    ${PROJECT_SOURCE_DIR}/engine/include
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

# Source Files
set(ENGINE_CORE_SOURCES
    engine/src/core/Engine.cpp
    engine/src/core/Scene.cpp
    engine/src/core/GameObject.cpp
    engine/src/core/Transform.cpp
)

set(ENGINE_ASSET_SOURCES
    engine/src/assets/AssetManager.cpp
    engine/src/assets/ProceduralObject.cpp
)

set(ENGINE_PHYSICS_SOURCES
    engine/src/physics/PhysicsWorld.cpp
)

set(ENGINE_EXISTING_SOURCES
    engine/src/core/surface_buffer_evaluator.cpp
    engine/src/core/mathematical_color_evaluator.cpp
    engine/src/core/bone_evaluator.cpp
    engine/src/core/anchorpoint_evaluator.cpp
    engine/src/core/surface_buffers.cpp
)
set(ALL_ENGINE_SOURCES
    ${ENGINE_CORE_SOURCES}
    ${ENGINE_ASSET_SOURCES}
    ${ENGINE_PHYSICS_SOURCES}
    ${ENGINE_EXISTING_SOURCES}
)

# Main Executable
add_executable(delirium
    engine/src/main.cpp
    ${ALL_ENGINE_SOURCES}
)

target_link_libraries(delirium
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
)

# Installation
install(TARGETS delirium DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/delirium/assets)

# Configuration Summary
message(STATUS "")
message(STATUS "=================================================")
message(STATUS "DeliRium Engine Configuration Summary")
message(STATUS "=================================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "=================================================")
message(STATUS "")
