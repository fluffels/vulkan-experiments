#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <string>
#include <vector>

#include "lib/meshes/Terrain.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <glm/glm.hpp>

#include "easylogging++.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "WangTiling.h"
#include "FS.h"
#include "Memory.h"
#include "Buffer.h"
#include "Vulkan.h"

struct Coord {
    double x;
    double y;
};

struct MVP {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Scene {
    Buffer indices;
    Buffer uniforms;
    Buffer vertices;
    MVP mvp;
    Image grassTexture;
    Image groundTexture;
	Image colour;
    Image depth;
    Image noise;
};

std::vector<TerrainVertex> groundVertices;
std::vector<uint32_t> indices;
Buffer groundBuffer;
Buffer groundIndexBuffer;
std::vector<uint32_t> groundIndexVector;
auto eye = glm::vec3(50.0f, -2.0f, 50.0f);
auto at = glm::vec3(0.0f, -2.0f, 0.0f);
auto up = glm::vec3(0.0f, 1.0f, 0.0f);
int keyboard[GLFW_KEY_LAST] = {GLFW_RELEASE};
const bool fullscreen = false;

const std::vector<const char*> requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int WINDOW_HEIGHT = 900;
const int WINDOW_WIDTH = 1800;

INITIALIZE_EASYLOGGINGPP

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char *layerPrefix,
    const char *msg,
    void *userData
) {
    if (flags == VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        LOG(ERROR) << "[" << layerPrefix << "] " << msg;
    } else if (flags == VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        LOG(WARNING) << "[" << layerPrefix << "] " << msg;
    } else {
        LOG(DEBUG) << "[" << layerPrefix << "] " << msg;
    }
    return VK_FALSE;
}

void onKeyEvent(
    GLFWwindow* window,
    int key,
    int scancode,
    int action,
    int mods
) {
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else {
        if (action == GLFW_PRESS) {
            keyboard[key] = GLFW_PRESS;
        } else if (action == GLFW_RELEASE) {
            keyboard[key] = GLFW_RELEASE;
        }
    }
    }

int
main (int argc, char** argv, char** envp) {
    START_EASYLOGGINGPP(argc, argv);

    VK vk;
    Scene scene;

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
        WINDOW_WIDTH, WINDOW_HEIGHT,
        "Vulkan Experiments",
        fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );
    if (!window) {
        glfwTerminate();
        return 2;
    }

    LOG(INFO) << "Swap to window context...";
    glfwMakeContextCurrent(window);

    /* NOTE(jan): Vulkan application. */
    VkApplicationInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "Vk Experiments";
    ai.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    ai.pEngineName = "No Engine";
    ai.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    ai.apiVersion = VK_API_VERSION_1_0;

    /* NOTE(jan): Start creating Vulkan instance. */
    VkInstanceCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &ai;

    /* NOTE(jan): Check whether validation layers should be enabled. */
#ifdef NDEBUG
    bool validation_enabled = false;
#else
	LOG(INFO) << "Enabling validation layers...";
    bool validationEnabled = true;
    std::vector<const char *> layersRequested = {
        "VK_LAYER_LUNARG_standard_validation"
    };
    {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        auto layersAvailable = new VkLayerProperties[count];
        vkEnumerateInstanceLayerProperties(&count, layersAvailable);
        for (const auto &nameRequested: layersRequested) {
            bool found = false;
            uint32_t i = 0;
            while ((i < count) && (!found)) {
                auto *nameAvailable = layersAvailable[i].layerName;
                if (strcmp(nameAvailable, nameRequested) == 0) {
                    found = true;
                }
                i++;
            }
            if (!found) {
                LOG(ERROR) << "Could not find layer '" << nameRequested
                           << "'.";
                LOG(WARNING) << "Disabling validation layers...";
                validationEnabled = false;
                break;
            }
        }
    }
#endif

    /* NOTE(jan): Conditionally enable validation layers. */
#ifndef NDEBUG
    if (validationEnabled) {
        ici.enabledLayerCount = static_cast<uint32_t>(layersRequested.size());
        ici.ppEnabledLayerNames = layersRequested.data();
    } else {
        ici.enabledLayerCount = 0;
    }
#endif

    /* NOTE(jan): Extensions. */
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(
            &glfwExtensionCount
    );
    std::vector<const char*> requestedExtensions;
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        requestedExtensions.push_back(glfwExtensions[i]);
    }
#ifndef NDEBUG
    if (validationEnabled) {
        requestedExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
#endif
    ici.enabledExtensionCount = static_cast<uint32_t>
        (requestedExtensions.size());
    ici.ppEnabledExtensionNames = requestedExtensions.data();

    /* NOTE(jan): Create Vulkan instance. */
    VkResult result = vkCreateInstance(&ici, nullptr, &vk.h);
    if (result == VK_ERROR_LAYER_NOT_PRESENT) {
        throw std::runtime_error("Layer not present.");
    } else if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        throw std::runtime_error("Extension not present.");
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not instantiate Vulkan.");
    }

    /* NOTE(jan): Debug callback. */
#ifndef NDEBUG
    VkDebugReportCallbackEXT callback_debug;
    if (validationEnabled) {
        VkDebugReportCallbackCreateInfoEXT cf = {};
        cf.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        cf.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                   VK_DEBUG_REPORT_WARNING_BIT_EXT |
                   VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        cf.pfnCallback = debugCallback;
        auto create =
            (PFN_vkCreateDebugReportCallbackEXT)
            vkGetInstanceProcAddr(vk.h, "vkCreateDebugReportCallbackEXT");
        if (create == nullptr) {
            LOG(WARNING) << "Could load debug callback creation function";
        } else {
            vkCheckSuccess(
                create(vk.h, &cf, nullptr, &callback_debug),
                "Could not create debug callback"
            );
        }
    }
#endif

    /* NOTE(jan): Create surface. */
    vkCheckSuccess(
        glfwCreateWindowSurface(vk.h, window, nullptr, &vk.surface),
        "Could not create surface."
    );
    glfwSetWindowPos(window, 10, 40);

    /* NOTE(jan): Physical device selection. */
    {
        uint32_t count;
        vkEnumeratePhysicalDevices(vk.h, &count, nullptr);
        if (count == 0) {
            throw std::runtime_error("No Vulkan devices detected.");
        }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(vk.h, &count, devices.data());
        int max_score = -1;
        for (const auto& device: devices) {
            int score = -1;
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceFeatures(device, &features);

            vkEnumerateDeviceExtensionProperties(
                device, nullptr, &count, nullptr
            );
            std::vector<VkExtensionProperties> extensions_available(count);
            vkEnumerateDeviceExtensionProperties(
                device, nullptr, &count, extensions_available.data()
            );
            std::set<std::string> extensions_required(
                requiredDeviceExtensions.begin(),
                requiredDeviceExtensions.end()
            );
            for (const auto& extension: extensions_available) {
                extensions_required.erase(extension.extensionName);
            }

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                device, vk.surface, &vk.swap.capabilities
            );
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                device, vk.surface, &count, nullptr
            );
            if (count > 0) {
                vk.swap.formats.resize(count);
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    device, vk.surface, &count,
                    vk.swap.formats.data()
                );
            }
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, vk.surface, &count, nullptr
            );
            if (count > 0) {
                vk.swap.modes.resize(count);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, vk.surface, &count,
                    vk.swap.modes.data()
                );
            }

			/* NOTE(jan): Determine our multisampling settings. */
			VkSampleCountFlags maxSampleCounts = std::min(
				properties.limits.framebufferColorSampleCounts,
				properties.limits.framebufferDepthSampleCounts
			);
			auto vkSampleCounts = {
				VK_SAMPLE_COUNT_2_BIT,
				VK_SAMPLE_COUNT_4_BIT,
				VK_SAMPLE_COUNT_8_BIT,
				VK_SAMPLE_COUNT_16_BIT,
				VK_SAMPLE_COUNT_32_BIT,
				VK_SAMPLE_COUNT_64_BIT,
			};
			std::vector<VkSampleCountFlagBits> availableSampleCounts;
			for (auto sampleCount : vkSampleCounts) {
				if (maxSampleCounts & sampleCount) {
					availableSampleCounts.push_back(sampleCount);
				}
			}
			vk.sampleCount = availableSampleCounts.back();

            if (!extensions_required.empty()) {
                LOG(ERROR) << properties.deviceName << " does not support "
                    << "all required extensions, skipping...";
            } else if (!features.geometryShader) {
                LOG(ERROR) << properties.deviceName << " does not support "
                    << "geometry shaders, skipping...";
            } else if (!features.samplerAnisotropy) {
                LOG(ERROR) << properties.deviceName << " does not support "
                    << "anisotropic samplers, skipping...";
            } else if (vk.swap.formats.empty()) {
                LOG(ERROR) << properties.deviceName << " does not support "
                    << "any compatible swap formats, skipping...";
            } else if (vk.swap.modes.empty()) {
                LOG(ERROR) << properties.deviceName << " does not support "
                    << "any compatible swap modes, skipping...";
            } else {
                vkGetPhysicalDeviceQueueFamilyProperties(
                    device, &count, nullptr
                );
                std::vector<VkQueueFamilyProperties> queueFamilies(count);
                vkGetPhysicalDeviceQueueFamilyProperties(
                    device, &count, queueFamilies.data()
                );
                vk.queues.graphics.family_index = -1;
                vk.queues.present.family_index = -1;
                int i = 0;
                for (const auto& queueFamily: queueFamilies) {
                    VkBool32 presentSupport = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(
                        device, i, vk.surface, &presentSupport
                    );
                    if ((queueFamily.queueCount) && presentSupport) {
                        vk.queues.present.family_index = i;
                    }
                    if ((queueFamily.queueCount) &&
                        (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                        vk.queues.graphics.family_index = i;
                    }
                    if ((vk.queues.present.family_index >= 0) &&
                        (vk.queues.graphics.family_index >= 0))
                    {
                        score = 0;
                        if (properties.deviceType ==
                            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                            score += 100;
                        }
                        break;
                    }
                    i++;
                }
            }
            LOG(INFO) << "Device '" << properties.deviceName
                      << "' scored at " << score;
            if (score > max_score) {
                vk.physical_device = device;
            }
            break;
        }
    }
    if (vk.physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable Vulkan devices detected.");
    }

    /* NOTE(jan): Logical device. */
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilyIndices = {
                vk.queues.graphics.family_index,
                vk.queues.present.family_index
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
        features.geometryShader = VK_TRUE;
        features.samplerAnisotropy = VK_TRUE;
		features.sampleRateShading = VK_TRUE;
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount =
                static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &features;
        createInfo.enabledExtensionCount =
                static_cast<uint32_t>(requiredDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
#ifndef NDEBUG
        if (validationEnabled) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(layersRequested.size());
            createInfo.ppEnabledLayerNames = layersRequested.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
#endif
        VkResult r = vkCreateDevice(
            vk.physical_device, &createInfo, nullptr, &vk.device
        );
        if (r == VK_ERROR_OUT_OF_HOST_MEMORY) {
            throw std::runtime_error("Out of host memory.");
        } else if (r == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            throw std::runtime_error("Out of device memory.");
        } else if (r == VK_ERROR_EXTENSION_NOT_PRESENT) {
            throw std::runtime_error("Extension not present.");
        } else if (r == VK_ERROR_FEATURE_NOT_PRESENT) {
            throw std::runtime_error("Feature not present.");
        } else if (r == VK_ERROR_TOO_MANY_OBJECTS) {
            throw std::runtime_error("Too many logical devices.");
        } else if (r == VK_ERROR_DEVICE_LOST) {
            throw std::runtime_error("Device lost.");
        }
        vkCheckSuccess(r, "Could not create physical device.");
    }

    /* NOTE(jan): Device queues. */
    {
        vkGetDeviceQueue(
            vk.device, vk.queues.graphics.family_index, 0, &vk.queues.graphics.q
        );
        vkGetDeviceQueue(
            vk.device, vk.queues.present.family_index, 0, &vk.queues.present.q
        );
    }

    /* NOTE(jan): Swap chain format. */
    {
        /* NOTE(jan): Pick a surface format. */
        /* NOTE(jan): Default. */
        vk.swap.format = vk.swap.formats[0];
        if ((vk.swap.formats.size() == 1) &&
            (vk.swap.formats[0].format == VK_FORMAT_UNDEFINED)) {
            vk.swap.format = {
                VK_FORMAT_B8G8R8A8_UNORM,
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
            LOG(INFO) << "Surface has no preferred format. "
                      << "Selecting 8 bit SRGB...";
        } else {
            for (const auto &format: vk.swap.formats) {
                if ((format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
                    (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) {
                    vk.swap.format = format;
                    LOG(INFO) << "Surface supports 8 bit SRGB. Selecting...";
                }
            }
        }
    }

    /* NOTE(jan): Swap chain presentation mode. */
    {
        /* NOTE(jan): Default. Guaranteed to be present. */
        vk.swap.mode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto &mode: vk.swap.modes) {
            /* NOTE(jan): This allows us to implement triple buffering. */
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                vk.swap.mode = VK_PRESENT_MODE_MAILBOX_KHR;
                LOG(INFO) << "Surface supports mailbox presentation mode. "
                          << "Selecting...";
            }
        }
    }

    /* NOTE(jan): Swap chain extent. */
    {
        if (vk.swap.capabilities.currentExtent.width !=
            std::numeric_limits<uint32_t>::max()) {
            vk.swap.extent = vk.swap.capabilities.currentExtent;
        } else {
            VkExtent2D extent = {WINDOW_WIDTH, WINDOW_HEIGHT};
            extent.width = std::max(
                vk.swap.capabilities.minImageExtent.width,
                std::min(
                    vk.swap.capabilities.maxImageExtent.width,
                    extent.width
                )
            );
            extent.height = std::max(
                vk.swap.capabilities.minImageExtent.height,
                std::min(
                    vk.swap.capabilities.maxImageExtent.height,
                    extent.height
                )
            );
            vk.swap.extent = extent;
        }
        LOG(INFO) << "Swap chain extent set to "
                  << vk.swap.extent.width
                  << "x"
                  << vk.swap.extent.height;
    }

    /* NOTE(jan): Swap chain length. */
    {
        vk.swap.l = vk.swap.capabilities.minImageCount + 1;
        /* NOTE(jan): maxImageCount == 0 means no limit. */
        if ((vk.swap.capabilities.maxImageCount < vk.swap.l) &&
                (vk.swap.capabilities.maxImageCount > 0)) {
            vk.swap.l = vk.swap.capabilities.maxImageCount;
        }
        vk.swap.images.resize(vk.swap.l);
        LOG(INFO) << "Swap chain length set to " << vk.swap.l;
    }

    /* NOTE(jan): Swap chain. */
    {
        VkSwapchainCreateInfoKHR cf = {};
        cf.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        cf.surface = vk.surface;
        cf.minImageCount = vk.swap.l;
        cf.imageFormat = vk.swap.format.format;
        cf.imageColorSpace = vk.swap.format.colorSpace;
        cf.imageExtent = vk.swap.extent;
        cf.imageArrayLayers = 1;
        cf.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        cf.preTransform = vk.swap.capabilities.currentTransform;
        cf.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        cf.presentMode = vk.swap.mode;
        cf.clipped = VK_TRUE;
        cf.oldSwapchain = VK_NULL_HANDLE;
        uint32_t queueFamilyIndices[] = {
            static_cast<uint32_t>(vk.queues.graphics.family_index),
            static_cast<uint32_t>(vk.queues.present.family_index)
        };
        if (vk.queues.graphics.family_index != vk.queues.present.family_index) {
            cf.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            cf.queueFamilyIndexCount = 2;
            cf.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            cf.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            cf.queueFamilyIndexCount = 0;
            cf.pQueueFamilyIndices = nullptr;
        }
        vkCheckSuccess(
            vkCreateSwapchainKHR(vk.device, &cf, nullptr, &vk.swap.h),
            "Could not create swap chain."
        );
    }

    /* NOTE(jan): Swap chain images. */
    {
        vkGetSwapchainImagesKHR(vk.device, vk.swap.h, &vk.swap.l, nullptr);
        auto images = new VkImage[vk.swap.l];
        vkGetSwapchainImagesKHR(vk.device, vk.swap.h, &vk.swap.l, images);
        for (uint32_t i = 0; i < vk.swap.l; i++) {
            vk.swap.images[i].i = images[i];
        }
        LOG(INFO) << "Retrieved " << vk.swap.l << " swap chain images.";
    }

    /* NOTE(jan): Swap chain image views. */
    for (uint32_t i = 0; i < vk.swap.l; i++) {
        VkImageViewCreateInfo cf = {};
        cf.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cf.image = vk.swap.images[i].i;
        cf.viewType = VK_IMAGE_VIEW_TYPE_2D;
        cf.format = vk.swap.format.format;
        cf.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        cf.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        cf.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        cf.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        cf.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cf.subresourceRange.baseMipLevel = 0;
        cf.subresourceRange.levelCount = 1;
        cf.subresourceRange.baseArrayLayer = 0;
        cf.subresourceRange.layerCount = 1;
        vkCheckSuccess(
             vkCreateImageView(vk.device, &cf, nullptr, &vk.swap.images[i].v),
             "Could not create image view."
        );
    }

    /* NOTE(jan): The render passes and descriptor sets below start the
     * pipeline creation process. */
    Pipeline grassPipeline = {};
	Pipeline groundPipeline = {};

    /* NOTE(jan): Render pass. */
    VkRenderPass defaultRenderPass;
    {
        VkAttachmentDescription descriptions[3] = {};
        descriptions[0].format = vk.swap.format.format;
        descriptions[0].samples = vk.sampleCount;
        descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        descriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        descriptions[1].format = vk.findDepthFormat();
        descriptions[1].samples = vk.sampleCount;
        descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        descriptions[1].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		descriptions[2].format = vk.swap.format.format;
		descriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
		descriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		descriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		descriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		descriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		descriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		descriptions[2].finalLayout =
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference refs[3] = {};
        refs[0].attachment = 0;
        refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        refs[1].attachment = 1;
        refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		refs[2].attachment = 2;
		refs[2].layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &refs[0];
        subpass.pDepthStencilAttachment = &refs[1];
		subpass.pResolveAttachments = &refs[2];

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
        cf.attachmentCount = 3;
        cf.pAttachments = descriptions;
        cf.subpassCount = 1;
        cf.pSubpasses = &subpass;
        cf.dependencyCount = 1;
        cf.pDependencies = &dep;

        vkCheckSuccess(
            vkCreateRenderPass(vk.device, &cf, nullptr, &defaultRenderPass),
            "Could not create render pass."
        );
    }

    VkDescriptorSetLayout defaultDescriptorSetLayout;
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        {
            VkDescriptorSetLayoutBinding b = {};
            b.binding = 0;
            b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b.descriptorCount = 1;
            b.stageFlags = (
                VK_SHADER_STAGE_VERTEX_BIT |
                VK_SHADER_STAGE_GEOMETRY_BIT
            );
            b.pImmutableSamplers = nullptr;
            bindings.push_back(b);
        }
        for (int i = 0; i < 2; i++) {
            VkDescriptorSetLayoutBinding b = {};
            b.binding = 1 + i;
            b.descriptorCount = 1;
            b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            b.pImmutableSamplers = nullptr;
            bindings.push_back(b);
        }
        {
            VkDescriptorSetLayoutBinding b = {};
            b.binding = 3;
            b.descriptorCount = 1;
            b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.stageFlags = (
                VK_SHADER_STAGE_GEOMETRY_BIT |
                VK_SHADER_STAGE_FRAGMENT_BIT
            );
            b.pImmutableSamplers = nullptr;
            bindings.push_back(b);
        }
        defaultDescriptorSetLayout = vk.createDescriptorSetLayout(bindings);
    }

	LOG(INFO) << "Creating grass pipeline...";
    {
        grassPipeline = vk.createPipeline<GridVertex>(
            "shaders/triangle",
            defaultRenderPass,
            defaultDescriptorSetLayout
        );
    }

	LOG(INFO) << "Creating ground pipeline...";
    {
        groundPipeline = vk.createPipeline<TerrainVertex>(
            "shaders/ground",
            defaultRenderPass,
            defaultDescriptorSetLayout
        );
    }

    /* NOTE(jan): Command pool creation. */
	LOG(INFO) << "Create command pools...";
    {
        VkCommandPoolCreateInfo cf = {};
        cf.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cf.queueFamilyIndex = vk.queues.graphics.family_index;
        cf.flags = 0;
        vkCheckSuccess(
            vkCreateCommandPool(vk.device, &cf, nullptr, &vk.commandPool),
            "Could not create command pool."
        );
    }

    /* NOTE(jan): Vertex buffers. */
    {
        Terrain terrain("noise.png");

        LOG(INFO) << "Generating model...";
        const int extent = 256;
        eye.x = 128;
        eye.y = terrain.getHeightAt(128, 128) - 1.f;
        eye.z = 128;

        std::vector<GridVertex> vertices;
        const float* positions = terrain.getPositions();
        const float* normals = terrain.getNormals();
        for (unsigned x = 0; x < terrain.getWidth(); x++) {
            for (unsigned z = 0; z < terrain.getDepth(); z++) {
                TerrainVertex v;
                v.pos = glm::vec3(positions[0], positions[1], positions[2]);
                v.normal = glm::vec3(normals[0], normals[1], normals[2]);
                v.tex = glm::vec2(
                    x / terrain.getWidth(),
                    z / terrain.getDepth()
                );
                groundVertices.push_back(v);
                positions += 3;
                normals += 3;
            }
        }
        groundBuffer = vk.createVertexBuffer<TerrainVertex>(groundVertices);

        const unsigned* groundIndices = terrain.getIndices();
        groundIndexVector.resize(terrain.getIndexCount());
        groundIndexVector.assign(
            groundIndices,
            groundIndices + terrain.getIndexCount()
        );
        groundIndexBuffer = vk.createIndexBuffer(groundIndexVector);

        const float density = 1.5;
        const int count = static_cast<int>(extent * density);
        WangTiling wangTiling(count, count);
        {
            for (int z = 0; z < count; z++) {
                for (int x = 0; x < count; x++) {
                    GridVertex vertex = {};
                    vertex.pos = {
                        x * (1/(float)density),
                        0.0f,
                        z * (1/(float)density),
                    };
                    vertex.type = wangTiling.getTile(z, x).getID();
                    indices.push_back(
                        static_cast<uint32_t>(vertices.size())
                    );
                    vertices.push_back(vertex);
                }
            }
        }
        scene.vertices = vk.createVertexBuffer<GridVertex>(vertices);
    }

    /* NOTE(jan): Index buffer. */
    {
        scene.indices = vk.createIndexBuffer(indices);
    }

    /* NOTE(jan): Uniform buffer. */
    {
        VkDeviceSize size = sizeof(scene.mvp);
        scene.uniforms = vk.createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            size
        );
    }

    scene.grassTexture = vk.createTexture("grass.png");
    scene.groundTexture = vk.createTexture("ground.png", true);
    scene.noise = vk.createTexture("noise.png");

	/* NOTE(jan): Colour buffer. */
	{
		scene.colour = vk.createImage(
			{ vk.swap.extent.width, vk.swap.extent.height, 1 },
			vk.sampleCount,
			vk.swap.format.format,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
        vk.transitionImage(
			scene.colour,
			vk.swap.format.format,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);
	}

    /* NOTE(jan): Depth buffer. */
    {
        auto format = vk.findDepthFormat();
        scene.depth = vk.createImage(
            {vk.swap.extent.width, vk.swap.extent.height, 1},
			vk.sampleCount,
            format,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
        vk.transitionImage(
            scene.depth,
            format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        );
    }

    /* NOTE(jan): Frame buffers. */
    {
        vk.swap.frames.resize(vk.swap.l);
        for (size_t i = 0; i < vk.swap.l; i++) {
            VkImageView attachments[] = {
				scene.colour.v,
                scene.depth.v,
                vk.swap.images[i].v,
            };
            VkFramebufferCreateInfo cf = {};
            cf.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            cf.renderPass = defaultRenderPass;
            cf.attachmentCount = 3;
            cf.pAttachments = attachments;
            cf.width = vk.swap.extent.width;
            cf.height = vk.swap.extent.height;
            cf.layers = 1;
            vkCheckSuccess(
                vkCreateFramebuffer(
                    vk.device, &cf, nullptr, &vk.swap.frames[i]
                ),
                "Could not create framebuffer."
            );
        }
    }

    /* NOTE(jan): Descriptor pool. */
    VkDescriptorPool defaultDescriptorPool;
    {
        std::vector<VkDescriptorPoolSize> size;
        {
            VkDescriptorPoolSize s = {};
            s.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            s.descriptorCount = 1;
            size.push_back(s);
        }
        for (int i = 0; i < 3; i++) {
            VkDescriptorPoolSize s = {};
            s.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            s.descriptorCount = 1;
            size.push_back(s);
        }
        defaultDescriptorPool = vk.createDescriptorPool(size);
    }

    /* NOTE(jan): Descriptor set. */
    VkDescriptorSet defaultDescriptorSet;
    {
        std::vector<VkDescriptorSetLayout> layouts;
        layouts.push_back(defaultDescriptorSetLayout);
        defaultDescriptorSet = vk.allocateDescriptorSet(
            defaultDescriptorPool, layouts
        );

        std::vector<VkWriteDescriptorSet> writes;
        {
            VkDescriptorBufferInfo i = {};
            i.buffer = scene.uniforms.buffer;
            i.offset = 0;
            i.range = sizeof(scene.mvp);
            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = defaultDescriptorSet;
            w.dstBinding = 0;
            w.dstArrayElement = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.descriptorCount = 1;
            w.pBufferInfo = &i;
            writes.push_back(w);
        }
        {
            VkDescriptorImageInfo i = {};
            i.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            i.imageView = scene.grassTexture.v;
            i.sampler = scene.grassTexture.s;
            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = defaultDescriptorSet;
            w.dstBinding = 1;
            w.dstArrayElement = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &i;
            writes.push_back(w);
        }
        {
            VkDescriptorImageInfo i = {};
            i.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            i.imageView = scene.groundTexture.v;
            i.sampler = scene.groundTexture.s;
            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = defaultDescriptorSet;
            w.dstBinding = 2;
            w.dstArrayElement = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &i;
            writes.push_back(w);
        }
        {
            VkDescriptorImageInfo i = {};
            i.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            i.imageView = scene.noise.v;
            i.sampler = scene.noise.s;
            VkWriteDescriptorSet w = {};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = defaultDescriptorSet;
            w.dstBinding = 3;
            w.dstArrayElement = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &i;
            writes.push_back(w);
        }

        vkUpdateDescriptorSets(
            vk.device,
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr
        );
    }

    /* NOTE(jan): Command buffer creation. */
    {
        vk.swap.command_buffers.resize(vk.swap.l);
        VkCommandBufferAllocateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        i.commandPool = vk.commandPool;
        i.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        i.commandBufferCount = (uint32_t)vk.swap.command_buffers.size();
        vkCheckSuccess(
            vkAllocateCommandBuffers(vk.device, &i, vk.swap.command_buffers.data()),
            "Could not allocate command buffers."
        );
    }

    /* NOTE(jan): Command buffer recording. */
    for (size_t i = 0; i < vk.swap.command_buffers.size(); i++) {
        VkCommandBufferBeginInfo cbbi = {};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        cbbi.pInheritanceInfo = nullptr;
        vkBeginCommandBuffer(vk.swap.command_buffers[i], &cbbi);

        /* NOTE(jan): Set up render pass. */
        VkRenderPassBeginInfo rpbi = {};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = defaultRenderPass;
        rpbi.framebuffer = vk.swap.frames[i];
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent = vk.swap.extent;
        VkClearValue clear[3] = {};
        clear[0].color = {1.0f, 1.0f, 1.0f, 0.0f};
        clear[1].depthStencil = {1.0f, 0};
        clear[2].color = {1.0f, 1.0f, 1.0f, 0.0f};
		rpbi.clearValueCount = 3;
        rpbi.pClearValues = clear;
        vkCmdBeginRenderPass(
            vk.swap.command_buffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE
        );
        vkCmdBindDescriptorSets(
            vk.swap.command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            grassPipeline.layout,
            0, 1,
            &defaultDescriptorSet,
            0, nullptr
        );
        VkDeviceSize offsets[] = {0};
        
        vkCmdBindPipeline(
            vk.swap.command_buffers[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
            groundPipeline.handle
        );
		VkBuffer ground_vertex_buffers[] = { groundBuffer.buffer };
		vkCmdBindVertexBuffers(
			vk.swap.command_buffers[i],
			0, 1,
			ground_vertex_buffers,
			offsets
		);
        vkCmdBindIndexBuffer(
            vk.swap.command_buffers[i], groundIndexBuffer.buffer,
            0, VK_INDEX_TYPE_UINT32
        );
		vkCmdDrawIndexed(
			vk.swap.command_buffers[i],
            static_cast<uint32_t>(groundIndexVector.size()),
			1, 0, 0, 0
		);

        vkCmdBindPipeline(
            vk.swap.command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
            grassPipeline.handle
        );
        VkBuffer vertex_buffers[] = {scene.vertices.buffer};
        vkCmdBindVertexBuffers(
            vk.swap.command_buffers[i], 0, 1, vertex_buffers, offsets
        );
        vkCmdBindIndexBuffer(
            vk.swap.command_buffers[i], scene.indices.buffer, 0, VK_INDEX_TYPE_UINT32
        );
        vkCmdDrawIndexed(
            vk.swap.command_buffers[i], static_cast<uint32_t>(indices.size()),
            1, 0, 0, 0
        );
        vkCmdEndRenderPass(vk.swap.command_buffers[i]);
        vkCheckSuccess(
            vkEndCommandBuffer(vk.swap.command_buffers[i]),
            "Failed to record command buffer."
        );
    }

    /* NOTE(jan): Create semaphores. */
    {
        VkSemaphoreCreateInfo sci = {};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkResult result;
        bool success;
        result = vkCreateSemaphore(
            vk.device, &sci, nullptr, &vk.swap.image_available
        );
        success = result == VK_SUCCESS;
        result = vkCreateSemaphore(
            vk.device, &sci, nullptr, &vk.swap.render_finished
        );
        success = success && (result == VK_SUCCESS);
        if (!success) {
            throw std::runtime_error("Could not create semaphores.");
        }
    }

    /* NOTE(jan): Initialize MVP matrices. */
    scene.mvp.model = glm::mat4(1.0f);
    scene.mvp.proj = glm::perspective(
        glm::radians(45.0f),
        vk.swap.extent.width / (float)vk.swap.extent.height,
        0.1f,
        1000.0f
    );

    /* NOTE(jan): Log frame times. */
    std::ofstream frameTimeFile("frames.csv", std::ios::out);
    frameTimeFile << "\"frameID\", \"ms_d\"" << std::endl;

    /* NOTE(jan): All calculations should be scaled by the time it took
     * to render the last frame. */
    auto start_f = std::chrono::high_resolution_clock::now();
    auto last_f = std::chrono::high_resolution_clock::now();
    auto this_f = std::chrono::high_resolution_clock::now();
    auto frame_count = 0;
    float total_delta_f = 0.0f;
    float delta_f = 0.0f;

    LOG(INFO) << "Entering main loop...";
    glfwSetKeyCallback(window, onKeyEvent);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		LOG(INFO) << "Raw mouse motion is supported, enabling...";
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
    Coord mouse_position_last_frame;
    glfwGetCursorPos(window, &mouse_position_last_frame.x, &mouse_position_last_frame.y);
    while(!glfwWindowShouldClose(window)) {
        last_f = std::chrono::high_resolution_clock::now();

        /* NOTE(jan): Copy MVP. */
        void* mvp_dst;
        size_t s = sizeof(scene.mvp);
        vkMapMemory(vk.device, scene.uniforms.memory, 0, s, 0, &mvp_dst);
            memcpy(mvp_dst, &scene.mvp, s);
        vkUnmapMemory(vk.device, scene.uniforms.memory);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            vk.device, vk.swap.h,
            std::numeric_limits<uint64_t>::max(),
            vk.swap.image_available, VK_NULL_HANDLE, &imageIndex
        );

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {vk.swap.image_available};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vk.swap.command_buffers[imageIndex];
        VkSemaphore signalSemaphores[] = {vk.swap.render_finished};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
		result = vkQueueSubmit(vk.queues.graphics.q, 1, &submitInfo, VK_NULL_HANDLE);
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY) {
			throw std::runtime_error("Could not submit to graphics queue: out of host memory.");
		} else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			throw std::runtime_error("Could not submit to graphics queue: out of device memory.");
		} else if (result == VK_ERROR_DEVICE_LOST) {
			throw std::runtime_error("Could not submit to graphics queue: device lost.");
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Could not submit to graphics queue for unknown reason.");
		};

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {vk.swap.h};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        /* NOTE(jan): For returning VkResults for multiple swap chains. */
        presentInfo.pResults = nullptr;

        vkQueuePresentKHR(vk.queues.present.q, &presentInfo);
        vkQueueWaitIdle(vk.queues.present.q);

        this_f = std::chrono::high_resolution_clock::now();
        delta_f = std::chrono::duration<
            float, std::chrono::seconds::period>(this_f - last_f).count();
        total_delta_f = std::chrono::duration<
            float, std::chrono::seconds::period>(this_f - start_f).count();

        frame_count++;
        frameTimeFile << frame_count << ", " << delta_f * 1000 << std::endl;

        float fps = 1.0f / delta_f;
        char title[255];
        snprintf(
            title, 255,
            "avg_ms_d: %f   ms_d: %f   FPS: %f",
            total_delta_f * 1000 / frame_count,
            delta_f*1000,
            fps
        );
        glfwSetWindowTitle(window, title);

        glfwPollEvents();
        /* NOTE(jan): Mouse look. */
        {
            double delta_angle = 3.14;
			Coord mouse_position_this_frame, mouse_delta;
			glfwGetCursorPos(
				window,
				&mouse_position_this_frame.x,
				&mouse_position_this_frame.y
			);
            mouse_delta = {
                mouse_position_this_frame.x - mouse_position_last_frame.x,
                mouse_position_this_frame.y - mouse_position_last_frame.y
            };
			mouse_position_last_frame = {
				mouse_position_this_frame.x,
				mouse_position_this_frame.y
			};
            Coord scaled = {
                mouse_delta.x / WINDOW_WIDTH,
                mouse_delta.y / WINDOW_HEIGHT
            };
            Coord rotation = {
				/* NOTE(jan): Positive rotation is clockwise. */
                scaled.x * delta_angle * (-1),
                scaled.y * delta_angle
            };
			glm::vec3 d(glm::normalize(at - eye));
			glm::vec3 right = glm::normalize(glm::cross(d, up));
			glm::mat4 rot(1.0f);
            rot = glm::rotate(rot, (float)rotation.y, right);
            rot = glm::rotate(rot, (float)rotation.x, up);
            glm::vec4 d4(d, 0.f);
            d4 = rot * d4;
            d = glm::normalize(glm::vec3(d4));
			d = glm::vec3(d4.x, d4.y, d4.z);
            at = eye + d;
        }

        /* NOTE(jan): Movement. */
        auto delta = 2.f;
        if (keyboard[GLFW_KEY_W] == GLFW_PRESS) {
            glm::vec3 forward = at - eye;
            forward.y = 0.0f;
            forward = glm::normalize(forward);
            eye += forward * delta * delta_f;
            at += forward * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_S] == GLFW_PRESS) {
            glm::vec3 backward = glm::normalize(eye - at);
            eye += backward * delta * delta_f;
            at += backward * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_A] == GLFW_PRESS) {
            glm::vec3 forward = glm::normalize(at - eye);
            glm::vec3 right = glm::cross(forward, up);
            glm::vec3 left = right * -1.f;
            eye += left * delta * delta_f;
            at += left * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_D] == GLFW_PRESS) {
            glm::vec3 forward = glm::normalize(at - eye);
            glm::vec3 right = glm::cross(forward, up);
            eye += right * delta * delta_f;
            at += right * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_Z] == GLFW_PRESS) {
            eye.x = 100;
            eye.y = -1;
            eye.z = 100;
            at.x = 0;
            at.z = 0;
            glm::vec3 forward = glm::normalize(at - eye);
            glm::vec3 right = glm::cross(forward, up);
            eye += right * delta * delta_f;
            at += right * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_X] == GLFW_PRESS) {
            eye.x = 0;
            eye.y = -1;
            eye.z = 0;
            at.x = 100;
            at.z = 100;
            glm::vec3 forward = glm::normalize(at - eye);
            glm::vec3 right = glm::cross(forward, up);
            eye += right * delta * delta_f;
            at += right * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_SPACE] == GLFW_PRESS) {
            eye -= up * delta * delta_f;
            at -= up * delta * delta_f;
        }
        if (keyboard[GLFW_KEY_LEFT_SHIFT] == GLFW_PRESS) {
            glm::vec3 down = up * -1.f;
            eye -= down * delta * delta_f;
            at -= down * delta * delta_f;
        }

        scene.mvp.view = glm::lookAt(eye, at, up);

        if (keyboard[GLFW_KEY_P] == GLFW_PRESS) {
			LOG(INFO) << "eye(" << eye.x << " " << eye.y << " " << eye.z << ")";
			LOG(INFO) << "at(" << at.x << " " << at.y << " " << at.z << ")";
		}
    }

    frameTimeFile.close();

    /* NOTE(jan): Wait for everything to complete before we start destroying
     * stuff. */
    LOG(INFO) << "Received exit request. Completing in-progress frames...";
    vkDeviceWaitIdle(vk.device);

    /* NOTE(jan): Clean up Vulkan objects. */
    LOG(INFO) << "Cleaning up...";
    auto vkDestroyCallback =
            (PFN_vkDestroyDebugReportCallbackEXT)
            vkGetInstanceProcAddr(
                    vk.h,
                    "vkDestroyDebugReportCallbackEXT"
            );
#ifndef NDEBUG
    if (vkDestroyCallback != nullptr) {
        vkDestroyCallback(vk.h, callback_debug, nullptr);
    }
#endif
    vkDestroySemaphore(vk.device, vk.swap.render_finished, nullptr);
    vkDestroySemaphore(vk.device, vk.swap.image_available, nullptr);
	vkDestroyImageView(vk.device, scene.colour.v, nullptr);
	vkFreeMemory(vk.device, scene.colour.m, nullptr);
	vkDestroyImage(vk.device, scene.colour.i, nullptr);
    vkDestroyImageView(vk.device, scene.depth.v, nullptr);
    vkFreeMemory(vk.device, scene.depth.m, nullptr);
    vkDestroyImage(vk.device, scene.depth.i, nullptr);
    vkDestroySampler(vk.device, scene.grassTexture.s, nullptr);
    vkDestroyImageView(vk.device, scene.grassTexture.v, nullptr);
    vkDestroyImage(vk.device, scene.grassTexture.i, nullptr);
    vkDestroySampler(vk.device, scene.groundTexture.s, nullptr);
    vkDestroyImageView(vk.device, scene.groundTexture.v, nullptr);
    vkDestroyImage(vk.device, scene.groundTexture.i, nullptr);
    vkDestroySampler(vk.device, scene.noise.s, nullptr);
    vkDestroyImageView(vk.device, scene.noise.v, nullptr);
    vkDestroyImage(vk.device, scene.noise.i, nullptr);
    vkFreeMemory(vk.device, scene.grassTexture.m, nullptr);
    vkFreeMemory(vk.device, scene.groundTexture.m, nullptr);
    vkFreeMemory(vk.device, scene.noise.m, nullptr);
    vkFreeMemory(vk.device, scene.indices.memory, nullptr);
    vkDestroyBuffer(vk.device, scene.indices.buffer, nullptr);
    vkFreeMemory(vk.device, scene.vertices.memory, nullptr);
    vkDestroyBuffer(vk.device, scene.vertices.buffer, nullptr);
    vkFreeMemory(vk.device, groundBuffer.memory, nullptr);
    vkDestroyBuffer(vk.device, groundBuffer.buffer, nullptr);
    vkFreeMemory(vk.device, scene.uniforms.memory, nullptr);
    vkDestroyBuffer(vk.device, scene.uniforms.buffer, nullptr);
    vkDestroyDescriptorPool(
        vk.device, defaultDescriptorPool, nullptr
    );
    vkDestroyCommandPool(vk.device, vk.commandPool, nullptr);
    for (const auto& f: vk.swap.frames) {
        vkDestroyFramebuffer(vk.device, f, nullptr);
    }
    vkDestroyPipeline(vk.device, grassPipeline.handle, nullptr);
    vkDestroyPipelineLayout(vk.device, grassPipeline.layout, nullptr);
    vkDestroyDescriptorSetLayout(
        vk.device, defaultDescriptorSetLayout, nullptr
    );
    vkDestroyRenderPass(vk.device, defaultRenderPass, nullptr);
    vkDestroyPipeline(vk.device, groundPipeline.handle, nullptr);
    vkDestroyPipelineLayout(vk.device, groundPipeline.layout, nullptr);
    for (const auto& i: vk.swap.images) {
        vkDestroyImageView(vk.device, i.v, nullptr);
    }
    vkDestroySwapchainKHR(vk.device, vk.swap.h, nullptr);
    vkDestroyDevice(vk.device, nullptr);
    vkDestroySurfaceKHR(vk.h, vk.surface, nullptr);
    vkDestroyInstance(vk.h, nullptr);

    /* NOTE(jan): Clean up GLFW. */
    glfwDestroyWindow(window);
    glfwTerminate();

    LOG(INFO) << "Exiting cleanly.";
    return 0;
}
