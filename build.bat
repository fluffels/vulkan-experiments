@echo off
# set LIB=D:\Lib\VulkanSDK\1.1.92.1\Lib\;D:\Lib\glfw-3.3.bin.WIN64\lib-vc2019\;%LIB%
mkdir build
pushd build
cl /std:c++latest /I D:\Lib\glfw-3.3.bin.WIN64\include /I D:\Lib\VulkanSDK\1.1.92.1\Include /I D:\Lib\glm ../main.cpp /link /LIBPATH D:\Lib\VulkanSDK\1.1.92.1\Lib\vulkan-1.lib /LIBPATH D:\Lib\glfw-3.3.bin.WIN64\lib-vc2019\glfw3dll.lib
popd
