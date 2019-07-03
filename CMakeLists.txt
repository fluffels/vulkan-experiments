cmake_minimum_required (VERSION 3.7)
set (CMAKE_CXX_STANDARD 17)
project (Vulkan)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
add_executable (main main.cpp)
target_link_libraries (main GLFW)
target_link_libraries (main ${Vulkan_LIBRARIES})
add_custom_target(
        vert.spv
        WORKING_DIRECTORY file(TO_NATIVE_PATH shaders triangle)
        COMMAND file(TO_NATIVE_PATH $ENV{VULKAN_SDK} bin glslangValidator) -V shader.vert
)
add_dependencies(main vert.spv)
add_custom_target(
        geom.spv
        WORKING_DIRECTORY file(TO_NATIVE_PATH shaders triangle)
        COMMAND file(TO_NATIVE_PATH $ENV{VULKAN_SDK} bin glslangValidator) -V shader.geom
)
add_dependencies(main geom.spv)
add_custom_target(
        frag.spv
        WORKING_DIRECTORY file(TO_NATIVE_PATH shaders triangle)
        COMMAND file(TO_NATIVE_PATH $ENV{VULKAN_SDK} bin glslangValidator) -V shader.frag
)
add_dependencies(main frag.spv)
