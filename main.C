#include <vector>

#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "easylogging++.h"

const uint32_t requestedValidationLayerCount = 1;
const char* requestedValidationLayers[requestedValidationLayerCount] = {
        "VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
bool enableValidationLayers = false;
#else
bool enableValidationLayers = true;
#endif

const int WINDOW_HEIGHT = 600;
const int WINDOW_WIDTH = 800;

INITIALIZE_EASYLOGGINGPP

VkInstance instance;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char* layerPrefix,
        const char* msg,
        void* userData) {
    LOG(ERROR) << "validation layer: " << msg;
    return VK_FALSE;
}

void on_key_event(
        GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int
main (int argc, char** argv) {
    START_EASYLOGGINGPP(argc, argv);

    LOG(INFO) << "Initalizing GLFW...";
    if (!glfwInit()) {
        return 1;
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    }

    LOG(INFO) << "Creating window...";
    auto window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Experiments", nullptr, nullptr
    );
    if (!window) {
        glfwTerminate();
        return 2;
    }

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    LOG(INFO) << "Found " << extensionCount << " extensions...";

    LOG(INFO) << "Swap to window context...";
    glfwMakeContextCurrent(window);

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vk Experiments";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    /* NOTE(jan): Debug layers. */
    uint32_t availableLayerCount;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
    VkLayerProperties layerProperties[availableLayerCount];
    vkEnumerateInstanceLayerProperties(&availableLayerCount, layerProperties);
    for (int r = 0; r < requestedValidationLayerCount; r++) {
        auto requestedLayerName = requestedValidationLayers[r];
        bool found = false;
        int a = 0;
        while ((a < availableLayerCount) && (!found)) {
            auto* availableLayerName = layerProperties[a].layerName;
            if (strcmp(availableLayerName, requestedLayerName) == 0) {
                found = true;
            }
            a++;
        }
        if (!found) {
            LOG(ERROR) << "Could not find layer '" << requestedLayerName << "'.";
            LOG(WARNING) << "Disabling validation layers...";
            enableValidationLayers = false;
        }
    }
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = requestedValidationLayerCount;
        createInfo.ppEnabledLayerNames = requestedValidationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    /* NOTE(jan): Extensions. */
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(
            &glfwExtensionCount
    );
    std::vector<const char*> requestedExtensions;
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        requestedExtensions.push_back(glfwExtensions[i]);
    }
    if (enableValidationLayers) {
        requestedExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>
        (requestedExtensions.size());
    createInfo.ppEnabledExtensionNames = requestedExtensions.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    /* NOTE(jan): Debug callback. */
    if (enableValidationLayers) {
        VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {};
        debugReportCallbackCreateInfo.sType =
                VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debugReportCallbackCreateInfo.flags =
                VK_DEBUG_REPORT_ERROR_BIT_EXT |
                VK_DEBUG_REPORT_WARNING_BIT_EXT;
        debugReportCallbackCreateInfo.pfnCallback = debugCallback;
        auto create = (PFN_vkCreateDebugReportCallbackEXT)
                vkGetInstanceProcAddr(
                        instance, "vkCreateDebugReportCallbackEXT");
        VkDebugReportCallbackEXT callback;
        if (create == nullptr) {
            LOG(WARNING) << "Could not register debug callback";
        } else {
            create(instance, &debugReportCallbackCreateInfo, nullptr,
                   &callback);
        }
    }

    if (result == VK_ERROR_LAYER_NOT_PRESENT) {
        LOG(ERROR) << "Layer not present.";
    } else if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        LOG(ERROR) << "Extension not present.";
    } else if (result != VK_SUCCESS) {
        LOG(ERROR) << "Could not instantiate Vulkan.";
    } else {
        LOG(INFO) << "Entering main loop...";
        glfwSetKeyCallback(window, on_key_event);
        while(!glfwWindowShouldClose(window)) {
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

