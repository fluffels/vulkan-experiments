#include <iostream>
#include <set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "easylogging++.h"

struct SwapChain {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    VkSurfaceFormatKHR format;
    std::vector<VkPresentModeKHR> presentModes;
    VkPresentModeKHR presentMode;
    VkExtent2D extent;
    uint32_t length;
    VkSwapchainKHR handle;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
};
SwapChain swapChain;

const uint32_t requestedValidationLayerCount = 1;
const char* requestedValidationLayers[requestedValidationLayerCount] = {
        "VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
bool enableValidationLayers = false;
#else
bool enableValidationLayers = true;
#endif

VkDebugReportCallbackEXT callback_debug;
VkDevice device = VK_NULL_HANDLE;
VkQueue graphicsQueue = VK_NULL_HANDLE;
VkQueue presentationQueue = VK_NULL_HANDLE;
VkSurfaceKHR surface = VK_NULL_HANDLE;

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
    if (flags == VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        LOG(ERROR) << "[" << layerPrefix << "] " << msg;
    } else if (flags == VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        LOG(WARNING) << "[" << layerPrefix << "] " << msg;
    } else {
        LOG(DEBUG) << "[" << layerPrefix << "] " << msg;
    }
    return VK_FALSE;
}

void on_key_event(
        GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int
main (int argc, char** argv, char** envp) {
    START_EASYLOGGINGPP(argc, argv);

    while (*envp != 0) {
        char* env = *envp;
        if (strstr(env, "VULKAN") == env ||
                strstr(env, "VK") == env ||
                strstr(env, "LD_LIBRARY_PATH=") == env ||
                strstr(env, "PATH") == env) {
            LOG(DEBUG) << env;
        }
        envp++;
    }

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

    if (result == VK_ERROR_LAYER_NOT_PRESENT) {
        LOG(ERROR) << "Layer not present.";
    } else if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        LOG(ERROR) << "Extension not present.";
    } else if (result != VK_SUCCESS) {
        LOG(ERROR) << "Could not instantiate Vulkan.";
    }

    /* NOTE(jan): Debug callback. */
    if (enableValidationLayers) {
        VkDebugReportCallbackCreateInfoEXT cf = {};
        cf.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        cf.flags =
                VK_DEBUG_REPORT_ERROR_BIT_EXT |
                VK_DEBUG_REPORT_WARNING_BIT_EXT |
                VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        cf.pfnCallback = debugCallback;
        auto create =
                (PFN_vkCreateDebugReportCallbackEXT)
                vkGetInstanceProcAddr(
                        instance,
                        "vkCreateDebugReportCallbackEXT"
                );
        if (create == nullptr) {
            LOG(WARNING) << "Could load debug callback creation function";
        } else {
            VkResult r = create(instance, &cf, nullptr, &callback_debug);
            if (r != VK_SUCCESS) {
                LOG(WARNING) << "Could not create debug callback";
            }
        }
    }

    /* NOTE(jan): Create surface. */
    {
        VkResult r;
        r = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (r != VK_SUCCESS) {
            LOG(ERROR) << "Could not create surface.";
        }
    }

    /* NOTE(jan): Physical device selection. */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    uint32_t queueFamilyCount;
    int graphicsQueueFamilyIndex;
    int presentationQueueFamilyIndex;
    if (deviceCount == 0) {
        LOG(ERROR) << "No Vulkan devices detected.";
    } else {
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        int max_score = -1;
        for (const auto& device: devices) {
            int score = -1;
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(
                    device, nullptr, &extensionCount, nullptr
            );
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(
                    device, nullptr, &extensionCount, availableExtensions.data()
            );
            std::set<std::string> requiredExtensionSet(
                    requiredDeviceExtensions.begin(),
                    requiredDeviceExtensions.end()
            );
            for (const auto& extension: availableExtensions) {
                requiredExtensionSet.erase(extension.extensionName);
            }

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    device, surface, &swapChain.capabilities
            );
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                    device, surface, &formatCount, nullptr
            );
            if (formatCount > 0) {
                swapChain.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                        device, surface, &formatCount,
                        swapChain.formats.data()
                );
            }
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, surface, &presentModeCount, nullptr
            );
            if (presentModeCount > 0) {
                swapChain.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                        device, surface, &presentModeCount,
                        swapChain.presentModes.data()
                );
            }

            if (requiredExtensionSet.empty() &&
                    !swapChain.formats.empty() &&
                    !swapChain.presentModes.empty()) {
                vkGetPhysicalDeviceQueueFamilyProperties(
                        device, &queueFamilyCount, nullptr
                );
                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(
                        device, &queueFamilyCount, queueFamilies.data()
                );
                graphicsQueueFamilyIndex = -1;
                presentationQueueFamilyIndex = -1;
                int i = 0;
                for (const auto& queueFamily: queueFamilies) {
                    VkBool32 presentSupport = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(
                            device, i, surface, &presentSupport
                    );
                    if ((queueFamily.queueCount) & presentSupport) {
                        presentationQueueFamilyIndex = i;
                    }
                    if ((queueFamily.queueCount) &&
                        (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                        graphicsQueueFamilyIndex = i;
                    }
                    if ((presentationQueueFamilyIndex >= 0) &&
                        (graphicsQueueFamilyIndex >= 0))
                    {
                        score = 0;
                        if (deviceProperties.deviceType ==
                            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                            score += 100;
                        }
                        break;
                    }
                    i++;
                }
            }
            LOG(INFO) << "Device '" << device << "' scored at " << score;
            if (score > max_score) {
                physicalDevice = device;
            }
            break;
        }
    }

    /* NOTE(jan): Logical device. */
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG(ERROR) << "No suitable Vulkan devices detected.";
    } else {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilyIndices = {
                graphicsQueueFamilyIndex,
                presentationQueueFamilyIndex
        };
        float queuePriority = 1.0f;
        for (int queueFamilyIndex: uniqueQueueFamilyIndices) {
            VkDeviceQueueCreateInfo cf = {};
            cf.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            cf.queueFamilyIndex = queueFamilyIndex;
            cf.queueCount = 1;
            cf.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(cf);
        }
        VkPhysicalDeviceFeatures features = {};
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount =
                static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &features;
        createInfo.enabledExtensionCount =
                static_cast<uint32_t>(requiredDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = requestedValidationLayerCount;
            createInfo.ppEnabledLayerNames = requestedValidationLayers;
        } else {
            createInfo.enabledLayerCount = 0;
        }
        VkResult r;
        r = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
        if (r == VK_ERROR_OUT_OF_HOST_MEMORY) {
            LOG(ERROR) << "Out of host memory.";
        } else if (r == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            LOG(ERROR) << "Out of device memory.";
        } else if (r == VK_ERROR_EXTENSION_NOT_PRESENT) {
            LOG(ERROR) << "Extension not present.";
        } else if (r == VK_ERROR_FEATURE_NOT_PRESENT) {
            LOG(ERROR) << "Feature not present.";
        } else if (r == VK_ERROR_TOO_MANY_OBJECTS) {
            LOG(ERROR) << "Too many logical devices.";
        } else if (r == VK_ERROR_DEVICE_LOST) {
            LOG(ERROR) << "Device lost.";
        } else if (r != VK_SUCCESS) {
            LOG(ERROR) << "Could not create physical device: " << r;
        }
    }

    if (device != VK_NULL_HANDLE) {
        /* NOTE(jan): Device queues. */
        vkGetDeviceQueue(
                device, graphicsQueueFamilyIndex, 0, &graphicsQueue
        );
        vkGetDeviceQueue(
                device, presentationQueueFamilyIndex, 0, &presentationQueue
        );
        LOG(INFO) << "Graphics queue: " << graphicsQueue;
        LOG(INFO) << "Presentation queue: " << presentationQueue;

        /* NOTE(jan): Pick a surface format. */
        /* NOTE(jan): Default. */
        swapChain.format = swapChain.formats[0];
        if ((swapChain.formats.size() == 1) &&
            (swapChain.formats[0].format == VK_FORMAT_UNDEFINED)) {
            swapChain.format = {
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
            LOG(INFO) << "Surface has no preferred format. "
                      << "Selecting 8 bit SRGB...";
        } else {
            for (const auto &format: swapChain.formats) {
                if ((format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
                    (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) {
                    swapChain.format = format;
                    LOG(INFO) << "Surface supports 8 bit SRGB. Selecting...";
                }
            }
        }

        /* NOTE(jan): Pick a surface presentation mode. */
        /* NOTE(jan): Default. Guaranteed to be present. */
        swapChain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& mode: swapChain.presentModes) {
            /* NOTE(jan): This allows us to implement triple buffering. */
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapChain.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                LOG(INFO) << "Surface supports mailbox presentation mode. "
                          << "Selecting...";
            }
        }

        /* NOTE(jan): Pick a swap chain extent. */
        if (swapChain.capabilities.currentExtent.width !=
            std::numeric_limits<uint32_t>::max()) {
            swapChain.extent = swapChain.capabilities.currentExtent;
        } else {
            VkExtent2D extent = {WINDOW_WIDTH, WINDOW_HEIGHT};
            extent.width = std::max(
                    swapChain.capabilities.minImageExtent.width,
                    std::min(
                            swapChain.capabilities.maxImageExtent.width,
                            extent.width
                    )
            );
            extent.height = std::max(
                    swapChain.capabilities.minImageExtent.height,
                    std::min(
                            swapChain.capabilities.maxImageExtent.height,
                            extent.height
                    )
            );
            swapChain.extent = extent;
        }
        LOG(INFO) << "Swap chain extent set to "
                  << swapChain.extent.width
                  << "x"
                  << swapChain.extent.height;

        /* NOTE(jan): Swap chain length. */
        swapChain.length = swapChain.capabilities.minImageCount + 1;
        /* NOTE(jan): maxImageCount == 0 means no limit. */
        if ((swapChain.capabilities.maxImageCount < swapChain.length) &&
                (swapChain.capabilities.maxImageCount > 0)) {
            swapChain.length = swapChain.capabilities.maxImageCount;
        }
        LOG(INFO) << "Swap chain length set to " << swapChain.length;

        /* NOTE(jan): Create swap chain. */
        {
            VkSwapchainCreateInfoKHR cf = {};
            cf.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            cf.surface = surface;
            cf.minImageCount = swapChain.length;
            cf.imageFormat = swapChain.format.format;
            cf.imageColorSpace = swapChain.format.colorSpace;
            cf.imageExtent = swapChain.extent;
            cf.imageArrayLayers = 1;
            cf.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            cf.preTransform = swapChain.capabilities.currentTransform;
            cf.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            cf.presentMode = swapChain.presentMode;
            cf.clipped = VK_TRUE;
            cf.oldSwapchain = VK_NULL_HANDLE;
            uint32_t queueFamilyIndices[] = {
                    static_cast<uint32_t>(graphicsQueueFamilyIndex),
                    static_cast<uint32_t>(presentationQueueFamilyIndex)
            };
            if (graphicsQueueFamilyIndex != presentationQueueFamilyIndex) {
                cf.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                cf.queueFamilyIndexCount = 2;
                cf.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                cf.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                cf.queueFamilyIndexCount = 0;
                cf.pQueueFamilyIndices = nullptr;
            }
            VkResult r;
            r = vkCreateSwapchainKHR(device, &cf, nullptr, &swapChain.handle);
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create swap chain: " << r;
            }
            vkGetSwapchainImagesKHR(
                    device, swapChain.handle, &swapChain.length, nullptr
            );
            swapChain.images.resize(swapChain.length);
            vkGetSwapchainImagesKHR(
                    device, swapChain.handle, &swapChain.length,
                    swapChain.images.data()
            );
            LOG(INFO) << "Retrieved "
                      << swapChain.length
                      << " swap chain images.";

            /* NOTE(jan): Swap chain image views. */
            swapChain.imageViews.resize(swapChain.length);
            for (int i = 0; i < swapChain.length; i++) {
                VkImageViewCreateInfo cf = {};
                cf.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                cf.image = swapChain.images[i];
                cf.viewType = VK_IMAGE_VIEW_TYPE_2D;
                cf.format = swapChain.format.format;
                cf.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                cf.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                cf.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                cf.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                cf.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                cf.subresourceRange.baseMipLevel = 0;
                cf.subresourceRange.levelCount = 1;
                cf.subresourceRange.baseArrayLayer = 0;
                cf.subresourceRange.layerCount = 1;
                VkResult r;
                r = vkCreateImageView(
                        device, &cf, nullptr, &swapChain.imageViews[i]
                );
                if (r != VK_SUCCESS) {
                    LOG(ERROR) << "Could not create image view #" << i;
                }
            }
        }

        LOG(INFO) << "Entering main loop...";
        glfwSetKeyCallback(window, on_key_event);
        while(!glfwWindowShouldClose(window)) {
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    auto vkDestroyCallback =
            (PFN_vkDestroyDebugReportCallbackEXT)
            vkGetInstanceProcAddr(
                    instance,
                    "vkDestroyDebugReportCallbackEXT"
            );
    if (vkDestroyCallback != nullptr) {
        vkDestroyCallback(instance, callback_debug, nullptr);
    }

    for (const auto& v: swapChain.imageViews) {
        vkDestroyImageView(device, v, nullptr);
    }
    vkDestroySwapchainKHR(device, swapChain.handle, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

