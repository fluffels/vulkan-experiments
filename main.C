#include <fstream>
#include <iostream>
#include <set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "easylogging++.h"

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

VkBuffer vertexBuffer;
VkDeviceMemory vertexBufferMemory;

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
    std::vector<VkFramebuffer> framebuffers;
};
SwapChain swapChain;

struct Pipeline {
    VkShaderModule vertModule;
    VkShaderModule fragModule;
    VkPipelineLayout layout;
    VkRenderPass pass;
    VkPipeline handle;
};
Pipeline pipeline = {};

VkCommandPool commandPool;
std::vector<VkCommandBuffer> commandBuffers;

VkSemaphore imageAvailable;
VkSemaphore renderFinished;

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

static std::vector<char>
readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG(ERROR) << "Could not open " << path;
    }
    size_t size = (size_t)file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}

VkShaderModule
createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo c = {};
    c.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    c.codeSize = code.size();
    c.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    VkResult r;
    r = vkCreateShaderModule(
            device, &c, nullptr, &module
    );
    if (r != VK_SUCCESS) {
        LOG(ERROR) << "Could not create shader module.";
    }
    return module;
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

        /* NOTE(jan): Create render passes. */
        {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = swapChain.format.format;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout =
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            VkSubpassDependency dep = {};
            dep.srcSubpass = VK_SUBPASS_EXTERNAL;
            dep.dstSubpass = 0;
            dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dep.srcAccessMask = 0;
            dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo cf = {};
            cf.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            cf.attachmentCount = 1;
            cf.pAttachments = &colorAttachment;
            cf.subpassCount = 1;
            cf.pSubpasses = &subpass;
            cf.dependencyCount = 1;
            cf.pDependencies = &dep;

            VkResult r;
            r = vkCreateRenderPass(
                    device, &cf, nullptr, &pipeline.pass
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create render pass.";
            }
        }

        /* NOTE(jan): Create pipeline. */
        {
            auto code = readFile("shaders/triangle/vert.spv");
            pipeline.vertModule = createShaderModule(code);
            code = readFile("shaders/triangle/frag.spv");
            pipeline.fragModule = createShaderModule(code);

            VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
            vertStageCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertStageCreateInfo.module = pipeline.vertModule;
            vertStageCreateInfo.pName = "main";
            /* NOTE(jan): Below would allow us to customize behaviour at
             * compile time. */
            // vertStageCreateInfo.pSpecializationInfo = ;

            VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
            fragStageCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragStageCreateInfo.module = pipeline.fragModule;
            fragStageCreateInfo.pName = "main";

            VkPipelineShaderStageCreateInfo stages[] = {
                    vertStageCreateInfo,
                    fragStageCreateInfo
            };

            VkVertexInputBindingDescription vibd = {};
            vibd.binding = 0;
            vibd.stride = sizeof(Vertex);
            vibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkVertexInputAttributeDescription viads[] = {{}, {}};
            viads[0].binding = 0;
            viads[0].location = 0;
            viads[0].format = VK_FORMAT_R32G32_SFLOAT;
            viads[0].offset = offsetof(Vertex, pos);

            viads[1].binding = 0;
            viads[1].location = 1;
            viads[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            viads[1].offset = offsetof(Vertex, color);

            VkPipelineVertexInputStateCreateInfo visci = {};
            visci.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            visci.vertexBindingDescriptionCount = 1;
            visci.pVertexBindingDescriptions = &vibd;
            visci.vertexAttributeDescriptionCount = 2;
            visci.pVertexAttributeDescriptions = viads;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
            inputAssembly.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)swapChain.extent.width;
            viewport.height = (float)swapChain.extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.offset = {0, 0};
            scissor.extent = swapChain.extent;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            VkPipelineRasterizationStateCreateInfo rasterizer = {};
            rasterizer.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            /* NOTE(jan): VK_TRUE is useful for shadow maps. */
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f;
            rasterizer.depthBiasClamp = 0.0f;
            rasterizer.depthBiasSlopeFactor = 0.0f;

            VkPipelineMultisampleStateCreateInfo multisampling = {};
            multisampling.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1.0f;
            multisampling.pSampleMask = nullptr;
            multisampling.alphaToCoverageEnable = VK_FALSE;
            multisampling.alphaToOneEnable = VK_FALSE;

            VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                  VK_COLOR_COMPONENT_G_BIT |
                                                  VK_COLOR_COMPONENT_B_BIT |
                                                  VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo colorBlending = {};
            colorBlending.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f;
            colorBlending.blendConstants[1] = 0.0f;
            colorBlending.blendConstants[2] = 0.0f;
            colorBlending.blendConstants[3] = 0.0f;

            /* NOTE(jan): This is for uniform values. */
            VkPipelineLayoutCreateInfo layoutCreateInfo = {};
            layoutCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutCreateInfo.setLayoutCount = 0;
            layoutCreateInfo.pSetLayouts = nullptr;
            layoutCreateInfo.pushConstantRangeCount = 0;
            layoutCreateInfo.pPushConstantRanges = nullptr;
            VkResult r;
            r = vkCreatePipelineLayout(
                    device, &layoutCreateInfo, nullptr, &pipeline.layout
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create pipeline layout.";
            }

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType =
                    VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = stages;
            pipelineInfo.pVertexInputState = &visci;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = nullptr;
            pipelineInfo.layout = pipeline.layout;
            pipelineInfo.renderPass = pipeline.pass;
            pipelineInfo.subpass = 0;
            /* NOTE(jan): Used to derive pipelines. */
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = -1;

            r = vkCreateGraphicsPipelines(
                    device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                    &pipeline.handle
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create graphics pipeline: " << r;
            }

            vkDestroyShaderModule(device, pipeline.fragModule, nullptr);
            vkDestroyShaderModule(device, pipeline.vertModule, nullptr);
        }

        /* NOTE(jan): Framebuffer. */
        {
            swapChain.framebuffers.resize(swapChain.length);
            for (size_t i = 0; i < swapChain.length; i++) {
                VkImageView attachments[] = {
                        swapChain.imageViews[i]
                };
                VkFramebufferCreateInfo cf = {};
                cf.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                cf.renderPass = pipeline.pass;
                cf.attachmentCount = 1;
                cf.pAttachments = attachments;
                cf.width = swapChain.extent.width;
                cf.height = swapChain.extent.height;
                cf.layers = 1;
                VkResult r = vkCreateFramebuffer(
                        device, &cf, nullptr, &swapChain.framebuffers[i]
                );
                if (r != VK_SUCCESS) {
                    LOG(ERROR) << "Could not create framebuffer.";
                }
            }
        }

        /* NOTE(jan): Command pool creation. */
        {
            VkCommandPoolCreateInfo cf = {};
            cf.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            cf.queueFamilyIndex = graphicsQueueFamilyIndex;
            cf.flags = 0;
            VkResult r = vkCreateCommandPool(
                    device, &cf, nullptr, &commandPool
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create command pool.";
            }
        }

        /* NOTE(jan): Vertex buffers. */
        {
            VkBufferCreateInfo bci = {};
            bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bci.size = sizeof(vertices[0]) * vertices.size();
            bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            VkResult r = vkCreateBuffer(
                    device, &bci, nullptr, &vertexBuffer
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create vertex buffer: " << r;
            }

            VkMemoryRequirements mr;
            vkGetBufferMemoryRequirements(device, vertexBuffer, &mr);

            VkPhysicalDeviceMemoryProperties pdmp;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &pdmp);

            VkBool32 found = VK_FALSE;
            uint32_t memory_type = 0;
            auto typeFilter = mr.memoryTypeBits;
            auto propertyFilter = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            for (uint32_t i = 0; i < pdmp.memoryTypeCount; i++) {
                auto& type = pdmp.memoryTypes[i];
                if ((typeFilter & (i << i)) &&
                        (type.propertyFlags & propertyFilter)) {
                    found = true;
                    memory_type = i;
                    break;
                }
            }

            if (!found) {
                LOG(ERROR) << "Could not find suitable memory for vertex "
                        "buffer";
            }

            VkMemoryAllocateInfo mai = {};
            mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            mai.allocationSize = mr.size;
            mai.memoryTypeIndex = memory_type;

            r = vkAllocateMemory(device, &mai, nullptr, &vertexBufferMemory);
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not allocate vertex buffer memory: " << r;
            } else {
                vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);
            }

            void* data;
            vkMapMemory(device, vertexBufferMemory, 0, bci.size, 0, &data);
            memcpy(data, vertices.data(), (size_t)bci.size);
            vkUnmapMemory(device, vertexBufferMemory);
        }

        /* NOTE(jan): Command buffer creation. */
        {
            commandBuffers.resize(swapChain.length);
            VkCommandBufferAllocateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            i.commandPool = commandPool;
            i.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            i.commandBufferCount = (uint32_t)commandBuffers.size();
            VkResult r = vkAllocateCommandBuffers(
                    device, &i, commandBuffers.data()
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not allocate command buffers: " << r;
            }
        }

        /* NOTE(jan): Command buffer recording. */
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo cbbi = {};
            cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cbbi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            cbbi.pInheritanceInfo = nullptr;
            vkBeginCommandBuffer(commandBuffers[i], &cbbi);
            /* NOTE(jan): Set up render pass. */
            VkRenderPassBeginInfo rpbi = {};
            rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpbi.renderPass = pipeline.pass;
            rpbi.framebuffer = swapChain.framebuffers[i];
            rpbi.renderArea.offset = {0, 0};
            rpbi.renderArea.extent = swapChain.extent;
            VkClearValue clear = {0.0f, 0.0f, 0.1f, 1.0f};
            rpbi.clearValueCount = 1;
            rpbi.pClearValues = &clear;
            vkCmdBeginRenderPass(
                    commandBuffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                    commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline.handle
            );
            vkCmdDraw(
                    commandBuffers[i], 3, 1, 0, 0
            );
            vkCmdEndRenderPass(
                    commandBuffers[i]
            );
            VkResult r = vkEndCommandBuffer(commandBuffers[i]);
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Failed to record command buffer #"
                           << i << ": " << r;
            }
        }

        /* NOTE(jan): Create semaphores. */
        {
            VkSemaphoreCreateInfo sci = {};
            sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkResult result;
            bool success;
            result = vkCreateSemaphore(
                    device, &sci, nullptr, &imageAvailable
            );
            success = result == VK_SUCCESS;
            result = vkCreateSemaphore(
                    device, &sci, nullptr, &renderFinished
            );
            success = success && (result == VK_SUCCESS);
            if (!success) {
                LOG(ERROR) << "Could not create semaphores.";
            }
        }

        LOG(INFO) << "Entering main loop...";
        glfwSetKeyCallback(window, on_key_event);
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            uint32_t imageIndex;
            vkAcquireNextImageKHR(
                    device, swapChain.handle,
                    std::numeric_limits<uint64_t>::max(),
                    imageAvailable, VK_NULL_HANDLE, &imageIndex
            );

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = {imageAvailable};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            VkPipelineStageFlags waitStages[] =
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
            VkSemaphore signalSemaphores[] = {renderFinished};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;
            VkResult r = vkQueueSubmit(
                    graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not submit to graphics queue: " << r;
            }

            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = {swapChain.handle};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;
            /* NOTE(jan): For returning VkResults for multiple swap chains. */
            presentInfo.pResults = nullptr;

            vkQueuePresentKHR(presentationQueue, &presentInfo);
            vkQueueWaitIdle(presentationQueue);
        }

        vkDeviceWaitIdle(device);
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
    vkDestroySemaphore(device, renderFinished, nullptr);
    vkDestroySemaphore(device, imageAvailable, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    for (const auto& f: swapChain.framebuffers) {
        vkDestroyFramebuffer(device, f, nullptr);
    }
    vkDestroyPipeline(device, pipeline.handle, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
    vkDestroyRenderPass(device, pipeline.pass, nullptr);
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
