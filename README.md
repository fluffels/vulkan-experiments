[![Build Status](https://travis-ci.org/fluffels/vulkan-experiments.svg?branch=master)](https://travis-ci.org/fluffels/vulkan-experiments)

# vulkan-experiments
Experimenting with the Vulkan graphics API.
This project is my implementation of the ideas from [GPU Gems 2.01](https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter01.html).
That article is a discussion on creating realistic natural scenes, which was first published in 2004.
It's quite old, and so contains a number of compromises that may not be necessary on current graphics hardware.
In part, I'm looking at how the techniques from that paper can be implemented on today's hardware and which constraints could be relaxed.

# Compilation
## Requirements
1. Install [VulkanSDK~=1.1.108.0](https://vulkan.lunarg.com/)
1. Install [glfw~=3.3](https://github.com/glfw/glfw/releases/tag/3.3)
1. Install [GLM~=0.9.9.5](https://github.com/g-truc/glm/releases/tag/0.9.9.5)

## Linux
1. `source /path/to/VulkanSDK/1.0.65.0/setup-env.sh`
1. `cmake .`
1. `make`

## Windows
1. Install Vulkan, glfw3 and glm.
2. Create `glfw3Config.cmake` containing lines such as these:
```
set(glfw3_INCLUDE_DIRS D:/Lib/glfw-3.3.bin.WIN64/include)
set(glfw3_LIBRARIES D:/Lib/glfw-3.3.bin.WIN64/lib-vc2019/glfw3dll.lib)
```
3. Create `glmConfig.cmake` containing a line such as this:
```
set(glm_INCLUDE_DIRS D:/Lib/glm)
```
4. Open directory in Visual Studio
5. Under `Project -> CMake Settings for <Project Name>` add the path to `glfw3Config.cmake` as the environment variable `glf3_DIR`.
6. Under `Project -> CMake Settings for <Project Name>` add the path to `glmConfig.cmake` as the environment variable `glm_DIR`.
7. Build

# Credits
1. Ground texture from [here](https://opengameart.org/content/grass-001).
2. Noise texture from [here](http://cpetry.github.io/TextureGenerator-Online/).
