[![Build Status](https://travis-ci.org/fluffels/vulkan-experiments.svg?branch=master)](https://travis-ci.org/fluffels/vulkan-experiments)

# vulkan-experiments
Experimenting with the Vulkan graphics API.
This project is my implementation of the ideas from [GPU Gems 2.01](https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter01.html).
That article is a discussion on creating realistic natural scenes, which was first published in 2004.
It's quite old, and so contains a number of compromises that may not be necessary on current graphics hardware.
In part, I'm looking at how the techniques from that paper can be implemented on today's hardware and which constraints could be relaxed.

# Compilation
## Linux
1. `source /path/to/VulkanSDK/1.0.65.0/setup-env.sh`
1. `cmake .`
1. `make`

## Windows
1. Install Vulkan, glfw3 and glm.
1. Create `glfw3Config.cmake` containing lines such as these:
```
set(glfw3_INCLUDE_DIRS D:/Lib/glfw-3.3.bin.WIN64/include)
set(glfw3_LIBRARIES D:/Lib/glfw-3.3.bin.WIN64/lib-vc2019/glfw3dll.lib)
```
1. Create `glmConfig.cmake` containing a line such as this:
```
set(glm_INCLUDE_DIRS D:/Lib/glm)
```
1. Open directory in Visual Studio
1. Under `Project -> CMake Settings for <Project Name>` add the path to `glfw3Config.cmake` as the environment variable `glf3_DIR`.
1. Under `Project -> CMake Settings for <Project Name>` add the path to `glmConfig.cmake` as the environment variable `glm_DIR`.
1. Build

# Credits
1. Ground texture from [here](https://opengameart.org/content/grass-001).
2. Noise texture from [here](http://cpetry.github.io/TextureGenerator-Online/).
