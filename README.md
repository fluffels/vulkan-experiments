[![Build Status](https://travis-ci.org/fluffels/vulkan-experiments.svg?branch=master)](https://travis-ci.org/fluffels/vulkan-experiments)

# vulkan-experiments
Experimenting with the Vulkan graphics API.
This project is my implementation of the ideas from [GPU Gems 2.01](https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter01.html).
That article is a discussion on creating realistic natural scenes, which was first published in 2004.
It's quite old, and so contains a number of compromises that may not be necessary on current graphics hardware.
In part, I'm looking at how the techniques from that paper can be implemented on today's hardware and which constraints could be relaxed.

## 1.2 The Grass Layer
The authors describe a rendering approach where a vertex and index buffer is prepared for the vertex shader that then arranges vertices into screen-aligned billboards.
This step can easily be simplified with contemporary graphics cards by leveraging the geometry shader.
Instead of passing every vertex and offsetting it using the vertex shader, I only pass in the grid coordinates for each grass clump.
The geometry shader then creates a screen-aligned billboard at each such grid coordinate.
*TODO:*
1. A mesh shader might be an even simpler approach, additionally I could cull grid coordinates even before billboards are generated for them.

I implemented the dissolve technique similarly to what's described in the paper.
It's a little simpler on new graphics hardware since the alpha test can be implemented in the fragment shader using the `discard;` command.
At the moment, I'm checking if the current texel is mostly transparent, if it is then it is `discard;`ed so that fragments from billboards behind the current one can fill it instead.
This makes the transparency effect look mostly order-independent.
I'm also checking the current texel on the noise texture.
If that is below a certain value the current fragment is also `discard;`ed.
This creates the "screen-door" effect mentioned in the paper, which basically removes the need to sort polygons for transparency.
*TODO:*
1. Feather edges.
2. Distance-from-camera based alpha test values.

I implemented the colour-variation as described in the paper, re-using the same noise texture that I used for the screen-door effect.
I also randomly flipped some textures around to break up repetition.
*TODO:*
1. Use Wang tiling to select different textures.
2. Width / height variation.

Lighting and wind are still unimplemented at time of writing.

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
1. Remember to set Vulkan_LIBRARY and Vulkan_INCLUDE_DIR
1. Create `glfw3Config.cmake` containing lines such as these:
```
set(glfw3_INCLUDE_DIRS D:/Lib/glfw-3.3.bin.WIN64/include)
set(glfw3_LIBRARIES D:/Lib/glfw-3.3.bin.WIN64/lib-vc2019/glfw3dll.lib)
```
1. Create `glmConfig.cmake` containing a line such as this:
```
set(glm_INCLUDE_DIRS D:/Lib/glm)
```
1. Copy libglfw.dll to `C:\Windows\System32`
1. Open directory in Visual Studio
1. Under `Project -> CMake Settings for <Project Name>` add the path to `glfw3Config.cmake` as the environment variable `glf3_DIR`.
1. Under `Project -> CMake Settings for <Project Name>` add the path to `glmConfig.cmake` as the environment variable `glm_DIR`.
1. Build

# Credits
1. Grass texture from [here](https://opengameart.org/content/grass-pack-03).
1. Ground texture from [here](https://opengameart.org/content/grass-001).
1. Noise texture from [here](http://cpetry.github.io/TextureGenerator-Online/).
