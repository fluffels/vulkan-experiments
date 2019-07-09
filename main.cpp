#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <string>
#include <vector>

#ifndef NOMINMAX
# define NOMINMAX
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "easylogging++.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "FS.h"
#include "Memory.h"
#include "NotImplementedError.h"
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
    Image texture;
	Image colour;
    Image depth;
};

std::vector<Vertex> vertices;
std::vector<uint32_t> indices;
std::vector<Vertex> groundVertices;
Buffer groundBuffer;
auto eye = glm::vec3(4.84618, -1.91234, 4.54172);
auto at = glm::vec3(5.45624, -1.52875, 5.23503);
auto up = glm::vec3(0.0f, 1.0f, 0.0f);
int keyboard[GLFW_KEY_LAST] = {GLFW_RELEASE};

const std::vector<const char*> requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int WINDOW_HEIGHT = 600;
const int WINDOW_WIDTH = 800;

INITIALIZE_EASYLOGGINGPP

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugReportFlagsEXT flags,
               VkDebugReportObjectTypeEXT objType,
               uint64_t obj,
               size_t location,
               int32_t code,
               const char *layerPrefix,
               const char *msg,
               void *userData) {
    if (flags == VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        LOG(ERROR) << "[" << layerPrefix << "] " << msg;
    } else if (flags == VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        LOG(WARNING) << "[" << layerPrefix << "] " << msg;
    } else {
        LOG(DEBUG) << "[" << layerPrefix << "] " << msg;
    }
    return VK_FALSE;
}

void on_key_event(GLFWwindow* window,
                  int key,
                  int scancode,
                  int action,
                  int mods) {
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

VkFormat
format_select_best_supported(VK& vk,
                             const std::vector<VkFormat>& candidates,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features) {
    for (VkFormat format: candidates) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(
            vk.physical_device, format, &properties
        );
        VkFormatFeatureFlags available_features;
        if (tiling == VK_IMAGE_TILING_LINEAR) {
            available_features = properties.linearTilingFeatures;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL) {
            available_features = properties.optimalTilingFeatures;
        }
        if (available_features & features) {
            return format;
        }
    }
    throw std::runtime_error("could not select an appropriate format");
}

bool
format_has_stencil(VkFormat format) {
    bool result;
    result = (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
    result = result ||(format == VK_FORMAT_D24_UNORM_S8_UINT);
    return result;
}

VkFormat
format_find_depth(VK& vk) {
    auto candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    auto tiling = VK_IMAGE_TILING_OPTIMAL;
    auto features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    return format_select_best_supported(vk, candidates, tiling, features);
}

Image
image_create(VK& vk,
             VkExtent3D extent,
			 VkSampleCountFlagBits sampleCount,
             VkFormat image_format,
             VkFormat view_format,
             VkImageTiling tiling,
             VkImageUsageFlags usage,
             VkMemoryPropertyFlags properties,
             VkImageAspectFlags aspects) {
    VkImageCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = extent;
	ici.samples = sampleCount;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = image_format;
    ici.tiling = tiling;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Image image = {};
    VkResult r = vkCreateImage(vk.device, &ici, nullptr, &image.i);
    if (r != VK_SUCCESS) {
        throw std::runtime_error("Could not create image.");
    }

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(vk.device, image.i, &mr);

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = vk.findMemoryType(mr, properties);

    r = vkAllocateMemory(vk.device, &mai, nullptr, &image.m);
    if (r != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate image.");
    }

    vkBindImageMemory(vk.device, image.i, image.m, 0);

    VkImageViewCreateInfo ivci = {};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = image.i;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = view_format;
    ivci.subresourceRange.layerCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.aspectMask = aspects;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;

    r = vkCreateImageView(vk.device, &ivci, nullptr, &image.v);
    if (r != VK_SUCCESS) {
        throw std::runtime_error("Could not create image view.");
    }

    return image;
}

void
image_transition(const VK& vk,
                 const VkCommandPool cp,
                 const Image& image,
                 VkFormat format,
                 VkImageLayout old_layout,
                 VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.i;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format_has_stencil(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkPipelineStageFlags stage_src;
    VkPipelineStageFlags stage_dst;

    if ((old_layout == VK_IMAGE_LAYOUT_UNDEFINED) &&
        (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        stage_dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if ((old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) &&
               (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        stage_src = VK_PIPELINE_STAGE_TRANSFER_BIT;
        stage_dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if ((old_layout == VK_IMAGE_LAYOUT_UNDEFINED) &&
               (new_layout ==
                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        stage_dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else if ((old_layout == VK_IMAGE_LAYOUT_UNDEFINED) &&
	           (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		stage_dst = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	} else {
        throw std::invalid_argument("Unsupported layout transition.");
    }

    auto cb = vk.startCommand();
    vkCmdPipelineBarrier(
        cb,
        stage_src, stage_dst,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    vk.submitCommand(cb);
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

    LOG(INFO) << "Generating model...";
	const int extent = 10;
	const int density = 1;
	const int count = extent * density;
    {
        for (int z = 0; z < count; z++) {
            for (int x = 0; x < count; x++) {
                Vertex vertex = {};
                vertex.pos = {
                    x * (1/(float)density),
                    0.0f,
                    z * (1/(float)density),
                };
                indices.push_back(vertices.size());
                vertices.push_back(vertex);
            }
        }
    }
    Vertex v0, v1, v2, v3;
    v0.pos = glm::vec3(-1.0f, 0.0f, -1.0f);
	v1.pos = glm::vec3(11.0f, 0.0f, -1.0f);
	v2.pos = glm::vec3(-1.0f, 0.0f, 11.0f);
	v3.pos = glm::vec3(11.0f, 0.0f, 11.0f);
    groundVertices.push_back(v0);
    groundVertices.push_back(v1);
    groundVertices.push_back(v2);
    groundVertices.push_back(v3);

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
    bool validation_enabled = true;
    std::vector<const char *> layers_requested = {
        "VK_LAYER_LUNARG_standard_validation"
    };
    {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        auto layers_available = new VkLayerProperties[count];
        vkEnumerateInstanceLayerProperties(&count, layers_available);
        for (const auto &name_requested: layers_requested) {
            bool found = false;
            int a = 0;
            while ((a < count) && (!found)) {
                auto *name_available = layers_available[a].layerName;
                if (strcmp(name_available, name_requested) == 0) {
                    found = true;
                }
                a++;
            }
            if (!found) {
                LOG(ERROR) << "Could not find layer '" << name_requested
                           << "'.";
                LOG(WARNING) << "Disabling validation layers...";
                validation_enabled = false;
                break;
            }
        }
    }
#endif

    /* NOTE(jan): Conditionally enable validation layers. */
#ifndef NDEBUG
    if (validation_enabled) {
        ici.enabledLayerCount = layers_requested.size();
        ici.ppEnabledLayerNames = layers_requested.data();
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
    if (validation_enabled) {
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
    if (validation_enabled) {
        VkDebugReportCallbackCreateInfoEXT cf = {};
        cf.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        cf.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                   VK_DEBUG_REPORT_WARNING_BIT_EXT |
                   VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        cf.pfnCallback = debug_callback;
        auto create =
            (PFN_vkCreateDebugReportCallbackEXT)
            vkGetInstanceProcAddr(vk.h, "vkCreateDebugReportCallbackEXT");
        if (create == nullptr) {
            LOG(WARNING) << "Could load debug callback creation function";
        } else {
            vk_check_success(
                create(vk.h, &cf, nullptr, &callback_debug),
                "Could not create debug callback"
            );
        }
    }
#endif

    /* NOTE(jan): Create surface. */
    vk_check_success(
        glfwCreateWindowSurface(vk.h, window, nullptr, &vk.surface),
        "Could not create surface."
    );

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
        if (validation_enabled) {
            createInfo.enabledLayerCount = layers_requested.size();
            createInfo.ppEnabledLayerNames = layers_requested.data();
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
        vk_check_success(r, "Could not create physical device.");
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
        vk_check_success(
            vkCreateSwapchainKHR(vk.device, &cf, nullptr, &vk.swap.h),
            "Could not create swap chain."
        );
    }

    /* NOTE(jan): Swap chain images. */
    {
        vkGetSwapchainImagesKHR(vk.device, vk.swap.h, &vk.swap.l, nullptr);
        auto images = new VkImage[vk.swap.l];
        vkGetSwapchainImagesKHR(vk.device, vk.swap.h, &vk.swap.l, images);
        for (int i = 0; i < vk.swap.l; i++) {
            vk.swap.images[i].i = images[i];
        }
        LOG(INFO) << "Retrieved " << vk.swap.l << " swap chain images.";
    }

    /* NOTE(jan): Swap chain image views. */
    for (int i = 0; i < vk.swap.l; i++) {
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
        vk_check_success(
             vkCreateImageView(vk.device, &cf, nullptr, &vk.swap.images[i].v),
             "Could not create image view."
        );
    }

    /* NOTE(jan): The render passes and descriptor sets below start the
     * pipeline creation process. */
    Pipeline pipeline = {};
	Pipeline grassPipeline = {};

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
        descriptions[1].format = format_find_depth(vk);
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

        vk_check_success(
            vkCreateRenderPass(vk.device, &cf, nullptr, &defaultRenderPass),
            "Could not create render pass."
        );
    }

    /* NOTE(jan): Descriptor set. */
    VkDescriptorSetLayout defaultDescriptorSetLayout;
    {
        VkDescriptorSetLayoutBinding bindings[2];
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
        bindings[0].pImmutableSamplers = nullptr;
        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        i.bindingCount = 2;
        i.pBindings = bindings;

        vk_check_success(
            vkCreateDescriptorSetLayout(
                vk.device, &i, nullptr, &defaultDescriptorSetLayout
            ),
            "Could not create descriptor set layout."
        );
    }

    /* NOTE(jan): Create pipeline. */
	LOG(INFO) << "Creating billboard pipeline...";
    {
        pipeline = vk.createPipeline(
            "shaders/triangle",
            defaultRenderPass,
            defaultDescriptorSetLayout
        );
    }

    /* NOTE(jan): Create pipeline. */
	LOG(INFO) << "Creating grass pipeline...";
    {
        grassPipeline = vk.createPipeline(
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
        vk_check_success(
            vkCreateCommandPool(vk.device, &cf, nullptr, &vk.commandPool),
            "Could not create command pool."
        );
    }

    /* NOTE(jan): Vertex buffers. */
    {
        scene.vertices = vk.createVertexBuffer(vertices);
        groundBuffer = vk.createVertexBuffer(groundVertices);
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

    /* NOTE(jan): Texture buffer. */
    {
        int width;
        int height;
        int depth;
        stbi_uc* pixels = stbi_load(
            "grass_square.png", &width, &height, &depth, STBI_rgb_alpha
        );
        if (!pixels) {
            throw std::runtime_error("Could not load texture.");
        }
        auto length = width * height * 4;
        auto staging = vk.createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            length
        );
        void* data;
        auto dsize = (VkDeviceSize)length;
        vkMapMemory(vk.device, staging.memory, 0, dsize, 0, &data);
            memcpy(data, pixels, length);
        vkUnmapMemory(vk.device, staging.memory);
        stbi_image_free(pixels);

        scene.texture = image_create(
            vk,
            {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                1
            },
			VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        image_transition(
            vk,
            vk.commandPool,
            scene.texture,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        auto cb = vk.startCommand();
        VkBufferImageCopy bic = {};
        bic.bufferOffset = 0;
        bic.bufferRowLength = 0;
        bic.bufferImageHeight = 0;
        bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bic.imageSubresource.mipLevel = 0;
        bic.imageSubresource.baseArrayLayer = 0;
        bic.imageSubresource.layerCount = 1;
        bic.imageOffset = {0, 0};
        bic.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1
        };
        vkCmdCopyBufferToImage(
            cb,
            staging.buffer,
            scene.texture.i,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bic
        );
        vk.submitCommand(cb);

        image_transition(
            vk,
            vk.commandPool,
            scene.texture,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        vkDestroyBuffer(vk.device, staging.buffer, nullptr);
        vkFreeMemory(vk.device, staging.memory, nullptr);

        VkSamplerCreateInfo sci = {};
        sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.anisotropyEnable = VK_TRUE;
        sci.maxAnisotropy = 16;
        sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sci.unnormalizedCoordinates = VK_FALSE;
        sci.compareEnable = VK_FALSE;
        sci.compareOp = VK_COMPARE_OP_ALWAYS;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.mipLodBias = 0.0f;
        sci.minLod = 0.0f;
        sci.maxLod = 0.0f;

        vk_check_success(
            vkCreateSampler(vk.device, &sci, nullptr, &scene.texture.s),
            "Could not create image sampler."
        );
    }

	/* NOTE(jan): Colour buffer. */
	{
		scene.colour = image_create(
			vk,
			{ vk.swap.extent.width, vk.swap.extent.height, 1 },
			vk.sampleCount,
			vk.swap.format.format,
			vk.swap.format.format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
		image_transition(
			vk,
			vk.commandPool,
			scene.colour,
			vk.swap.format.format,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);
	}


    /* NOTE(jan): Depth buffer. */
    {
        auto format = format_find_depth(vk);
        scene.depth = image_create(
            vk,
            {vk.swap.extent.width, vk.swap.extent.height, 1},
			vk.sampleCount,
            format,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
        image_transition(
            vk,
            vk.commandPool,
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
            vk_check_success(
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
        VkDescriptorPoolSize dps[2] = {};
        dps[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dps[0].descriptorCount = 1;
        dps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dps[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo dpci = {};
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.poolSizeCount = 2;
        dpci.pPoolSizes = dps;
        dpci.maxSets = 1;

        vk_check_success(
            vkCreateDescriptorPool(
                vk.device, &dpci, nullptr, &defaultDescriptorPool
            ),
            "Could not create descriptor pool."
        );
    }

    /* NOTE(jan): Descriptor set. */
    VkDescriptorSet defaultDescriptorSet;
    {
        VkDescriptorSetLayout layouts[] = {defaultDescriptorSetLayout};
        VkDescriptorSetAllocateInfo dsai = {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = defaultDescriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = layouts;

        vk_check_success(
            vkAllocateDescriptorSets(
                vk.device, &dsai, &defaultDescriptorSet
            ),
            "Could not allocate descriptor set."
        );

        VkDescriptorBufferInfo dbi = {};
        dbi.buffer = scene.uniforms.buffer;
        dbi.offset = 0;
        dbi.range = sizeof(scene.mvp);

        VkDescriptorImageInfo dii = {};
        dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dii.imageView = scene.texture.v;
        dii.sampler = scene.texture.s;

        VkWriteDescriptorSet wds[2] = {};
        wds[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds[0].dstSet = defaultDescriptorSet;
        wds[0].dstBinding = 0;
        wds[0].dstArrayElement = 0;
        wds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds[0].descriptorCount = 1;
        wds[0].pBufferInfo = &dbi;
        wds[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds[1].dstSet = defaultDescriptorSet;
        wds[1].dstBinding = 1;
        wds[1].dstArrayElement = 0;
        wds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wds[1].descriptorCount = 1;
        wds[1].pImageInfo = &dii;

        vkUpdateDescriptorSets(vk.device, 2, wds, 0, nullptr);
    }

    /* NOTE(jan): Command buffer creation. */
    {
        vk.swap.command_buffers.resize(vk.swap.l);
        VkCommandBufferAllocateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        i.commandPool = vk.commandPool;
        i.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        i.commandBufferCount = (uint32_t)vk.swap.command_buffers.size();
        vk_check_success(
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
            pipeline.layout,
            0, 1,
            &defaultDescriptorSet,
            0, nullptr
        );
        VkDeviceSize offsets[] = {0};
        vkCmdBindPipeline(
            vk.swap.command_buffers[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
            grassPipeline.handle
        );
		VkBuffer ground_vertex_buffers[] = { groundBuffer.buffer };
		vkCmdBindVertexBuffers(
			vk.swap.command_buffers[i],
			0, 1,
			ground_vertex_buffers,
			offsets
		);
		vkCmdDraw(
			vk.swap.command_buffers[i], 4,
			1, 0, 0
		);

        vkCmdBindPipeline(
            vk.swap.command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline.handle
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
        vk_check_success(
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

    /* NOTE(jan): All calculations should be scaled by the time it took
     * to render the last frame. */
    auto last_f = std::chrono::high_resolution_clock::now();
    auto this_f = std::chrono::high_resolution_clock::now();
    float delta_f = 0.0f;

    LOG(INFO) << "Entering main loop...";
    glfwSetKeyCallback(window, on_key_event);
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

        float fps = 1.0f / delta_f;
        char title[255];
        snprintf(title, 255, "FPS: %f", fps);
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
            glm::vec3 forward = glm::normalize(at - eye);
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
    vkDestroySampler(vk.device, scene.texture.s, nullptr);
    vkDestroyImageView(vk.device, scene.texture.v, nullptr);
    vkDestroyImage(vk.device, scene.texture.i, nullptr);
    vkFreeMemory(vk.device, scene.texture.m, nullptr);
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
    vkDestroyPipeline(vk.device, pipeline.handle, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline.layout, nullptr);
    vkDestroyDescriptorSetLayout(
        vk.device, defaultDescriptorSetLayout, nullptr
    );
    vkDestroyRenderPass(vk.device, defaultRenderPass, nullptr);
    vkDestroyPipeline(vk.device, grassPipeline.handle, nullptr);
    vkDestroyPipelineLayout(vk.device, grassPipeline.layout, nullptr);
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
