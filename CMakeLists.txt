cmake_minimum_required (VERSION 3.7)
set (CMAKE_CXX_STANDARD 17)
project (Vulkan)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
include_directories(${glfw3_INCLUDE_DIRS})
include_directories(${glm_INCLUDE_DIRS})
include_directories(${Vulkan_INCLUDE_DIRS})
add_executable (main main.cpp WangTiling.cpp WangTile.cpp)
target_link_libraries (main ${Vulkan_LIBRARIES} ${glfw3_LIBRARIES})
set_target_properties(
    main PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
)
add_custom_target(
        triangle_vert
        WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/shaders/triangle
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V shader.vert
)
add_dependencies(main triangle_vert)
add_custom_target(
        triangle_geom
        WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/shaders/triangle
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V shader.geom
)
add_dependencies(main triangle_geom)
add_custom_target(
        triangle_frag
        WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/shaders/triangle
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V shader.frag
)
add_dependencies(main triangle_frag)
add_custom_target(
        ground_vert
        WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/shaders/ground
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V shader.vert
)
add_dependencies(main ground_vert)
add_custom_target(
        ground_frag
        WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/shaders/ground
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V shader.frag
)
add_dependencies(main ground_frag)
