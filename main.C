#include <cstdlib>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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

    LOG(INFO) << "Swap to window context...";
    glfwMakeContextCurrent(window);

    LOG(INFO) << "Entering main loop...";
    while(!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glm::mat4 matrix;

    glfwTerminate();
    return 0;
}

