@echo off
mkdir build
pushd build
cl /std:c++17 /I D:\Lib\glfw-3.3.bin.WIN64\include /I D:\Lib\VulkanSDK\1.1.92.1\Include /I D:\Lib\glm ../main.cpp /link D:\Lib\VulkanSDK\1.1.92.1\Lib\vulkan-1.lib D:\Lib\glfw-3.3.bin.WIN64\lib-vc2019\glfw3dll.lib
popd
pushd shaders\triangle
D:\Lib\VulkanSDK\1.1.92.1\Bin\glslangValidator -V shader.vert
D:\Lib\VulkanSDK\1.1.92.1\Bin\glslangValidator -V shader.geom
D:\Lib\VulkanSDK\1.1.92.1\Bin\glslangValidator -V shader.frag
popd
