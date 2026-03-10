cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/vcpkg.cmake)

project (VulkanPlayground)

message ("using c++ 20")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(ASSETS_PATH "${CMAKE_SOURCE_DIR}/assets")
set(SPV_PATH "${CMAKE_BINARY_DIR}/Spvs/")

set(RENDERING_API "VULKAN")

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(imgui REQUIRED)
find_package(Stb REQUIRED)
find_package(Taskflow REQUIRED)
find_package(plog REQUIRED)
find_package(flecs REQUIRED)

FIND_PATH(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

if(TINYGLTF_INCLUDE_DIRS STREQUAL "")
    message(FATAL_ERROR "Tiny not found")
endif()

# Enable folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

message ("\nConfiguring engine ..\n")
add_subdirectory(source/engine projects/core)

message ("\nConfiguring app ..")
add_subdirectory(source/app/gltf projects/apps/gltf)