#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

int
main (int argc, char** argv) {
    START_EASYLOGGINGPP(argc, argv);

    LOG(INFO) << "Initalizing GLFW...";
    if (!glfwInit()) {
        return 1;
    }

    LOG(INFO) << "Creating window...";
    auto window = glfwCreateWindow(640, 480, "Vulkan Experiments", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return 2;
    }

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    LOG(INFO) << "Swap to window context...";
    glfwMakeContextCurrent(window);

    LOG(INFO) << "Entering main loop...";
    while(!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

