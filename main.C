#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "easylogging++.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex;
};

struct VK {
    VkDevice device;
    VkPhysicalDevice physical_device;
};

struct {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
} mvp;

struct Buffer {
    VkBuffer b;
    VkDeviceMemory m;
};

struct Image {
    VkImage i;
    VkImageView v;
    VkDeviceMemory m;
    VkSampler s;
};

struct Scene {
    Buffer indices;
    Buffer vertices;
    Buffer mvp;
    Image texture;
    Image depth;
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

template<class T> size_t
vector_size(const std::vector<T>& v) {
    size_t result = sizeof(v[0]) * v.size();
    return result;
}

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
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
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

uint32_t
memory_find_type(VK& vk,
                 VkMemoryRequirements requirements,
                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties pdmp;
    vkGetPhysicalDeviceMemoryProperties(vk.physical_device, &pdmp);

    VkBool32 found = VK_FALSE;
    uint32_t memory_type = 0;
    auto typeFilter = requirements.memoryTypeBits;
    for (uint32_t i = 0; i < pdmp.memoryTypeCount; i++) {
        auto& type = pdmp.memoryTypes[i];
        auto check_type = typeFilter & (i << i);
        auto check_flags = type.propertyFlags & properties;
        if (check_type && check_flags) {
            found = true;
            memory_type = i;
            break;
        }
    }
    if (!found) {
        LOG(ERROR) << "Suitable buffer memory not found";
    }
    return memory_type;
}

Image
image_create(VK& vk,
             VkExtent3D extent,
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
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = image_format;
    ici.tiling = tiling;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;

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
    mai.memoryTypeIndex = memory_find_type(vk, mr, properties);

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

VkCommandBuffer
command_one_off_start(VK &vk) {
    /* TODO(jan): Create a separate command pool for short lived buffers and
     * set VK_COMMAND_POOL_CREATE_TRANSIENT_BIT so the implementation can
     * optimize this. */
    VkCommandBufferAllocateInfo cbai = {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandPool = commandPool;
    cbai.commandBufferCount = 1;

    VkCommandBuffer cb;
    vkAllocateCommandBuffers(vk.device, &cbai, &cb);

    VkCommandBufferBeginInfo cbbi = {};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cb, &cbbi);

    return cb;
}

void
command_one_off_stop_and_submit(VK& vk,
                                VkCommandBuffer& cb) {
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;

    vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(vk.device, commandPool, 1, &cb);
}

void
image_transition(VK& vk,
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
    } else {
        throw std::invalid_argument("Unsupported layout transition.");
    }

    auto cb = command_one_off_start(vk);
    vkCmdPipelineBarrier(
        cb,
        stage_src, stage_dst,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    command_one_off_stop_and_submit(vk, cb);
}

Buffer
buffer_create(VK& vk,
              VkBufferUsageFlags usage,
              VkMemoryPropertyFlags required_memory_properties,
              VkDeviceSize size) {
    Buffer buffer = {};

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult r = vkCreateBuffer(
            vk.device, &bci, nullptr, &buffer.b
    );
    if (r != VK_SUCCESS) {
        LOG(ERROR) << "Could not create buffer: " << r;
    }

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(vk.device, buffer.b, &mr);

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = memory_find_type(vk, mr, required_memory_properties);
    r = vkAllocateMemory(vk.device, &mai, nullptr, &buffer.m);
    if (r != VK_SUCCESS) {
        LOG(ERROR) << "Could not allocate buffer memory: " << r;
    } else {
        vkBindBufferMemory(vk.device, buffer.b, buffer.m, 0);
    }

    return buffer;
}

Buffer
buffer_create_and_initialize(
        VK &vk,
        VkBufferUsageFlags usage,
        VkDeviceSize size,
        void *contents) {
    auto staging = buffer_create(
            vk,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            size
    );

    void* data;
    vkMapMemory(vk.device, staging.m, 0, size, 0, &data);
    memcpy(data, contents, (size_t)size);
    vkUnmapMemory(vk.device, staging.m);

    auto buffer = buffer_create(
            vk,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            size
    );

    auto cb = command_one_off_start(vk);
    VkBufferCopy bc = {};
    bc.size = size;
    vkCmdCopyBuffer(cb, staging.b, buffer.b, 1, &bc);
    command_one_off_stop_and_submit(vk, cb);

    vkDestroyBuffer(vk.device, staging.b, nullptr);
    vkFreeMemory(vk.device, staging.m, nullptr);

    return buffer;
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
                    deviceFeatures.samplerAnisotropy &&
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
                vk.physical_device = device;
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
        features.samplerAnisotropy = VK_TRUE;
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
        } else {
            vk.device = device;
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
                    VK_FORMAT_B8G8R8A8_UNORM,
                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
            LOG(INFO) << "Surface has no preferred format. "
                      << "Selecting 8 bit SRGB...";
        } else {
            for (const auto &format: swapChain.formats) {
                if ((format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
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
            VkAttachmentDescription descriptions[2] = {};
            descriptions[0].format = swapChain.format.format;
            descriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
            descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            descriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            descriptions[1].format = format_find_depth(vk);
            descriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
            descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            descriptions[1].finalLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference refs[2] = {};
            refs[0].attachment = 0;
            refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            refs[1].attachment = 1;
            refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &refs[0];
            subpass.pDepthStencilAttachment = &refs[1];

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
            cf.attachmentCount = 2;
            cf.pAttachments = descriptions;
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

        /* NOTE(jan): Descriptor set. */
        {
            VkDescriptorSetLayoutBinding bindings[2];
            bindings[0].binding = 0;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            bindings[0].pImmutableSamplers = nullptr;
            bindings[1].binding = 1;
            bindings[1].descriptorCount = 1;
            bindings[1].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings[1].pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo dslci = {};
            dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            dslci.bindingCount = 2;
            dslci.pBindings = bindings;

            VkResult r = vkCreateDescriptorSetLayout(
                vk.device, &dslci, nullptr, &pipeline.descriptorSetLayout
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create descriptor set layout.";
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

            VkVertexInputAttributeDescription viads[3] = {};
            viads[0].binding = 0;
            viads[0].location = 0;
            viads[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            viads[0].offset = offsetof(Vertex, pos);
            viads[1].binding = 0;
            viads[1].location = 1;
            viads[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            viads[1].offset = offsetof(Vertex, color);
            viads[2].binding = 0;
            viads[2].location = 2;
            viads[2].format = VK_FORMAT_R32G32_SFLOAT;
            viads[2].offset = offsetof(Vertex, tex);

            VkPipelineVertexInputStateCreateInfo visci = {};
            visci.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            visci.vertexBindingDescriptionCount = 1;
            visci.pVertexBindingDescriptions = &vibd;
            visci.vertexAttributeDescriptionCount = 3;
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
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

            VkPipelineLayoutCreateInfo layoutCreateInfo = {};
            layoutCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutCreateInfo.setLayoutCount = 1;
            layoutCreateInfo.pSetLayouts = &pipeline.descriptorSetLayout;
            layoutCreateInfo.pushConstantRangeCount = 0;
            layoutCreateInfo.pPushConstantRanges = nullptr;
            VkResult r;
            r = vkCreatePipelineLayout(
                    device, &layoutCreateInfo, nullptr, &pipeline.layout
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create pipeline layout.";
            }

            VkPipelineDepthStencilStateCreateInfo dssci = {};
            dssci.sType =
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            dssci.depthTestEnable = VK_TRUE;
            dssci.depthWriteEnable = VK_TRUE;
            dssci.depthCompareOp = VK_COMPARE_OP_LESS;
            dssci.depthBoundsTestEnable = VK_FALSE;
            dssci.front = {};
            dssci.back = {};

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
            pipelineInfo.pDepthStencilState = &dssci;
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
            VkDeviceSize size = vector_size(vertices);
            scene.vertices = buffer_create_and_initialize(
                    vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, size,
                    (void *) vertices.data()
            );
        }

        /* NOTE(jan): Index buffer. */
        {
            VkDeviceSize size = vector_size(indices);
            scene.indices = buffer_create_and_initialize(
                    vk, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, size,
                    (void *) indices.data()
            );
        }

        /* NOTE(jan): Uniform buffer. */
        {
            VkDeviceSize size = sizeof(mvp);
            scene.mvp = buffer_create(
                vk,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
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
                "chalet.jpg", &width, &height, &depth, STBI_rgb_alpha
            );
            if (!pixels) {
                LOG(ERROR) << "Could not load texture.";
            }
            auto length = width * height * 4;
            auto staging = buffer_create(
                vk,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                length
            );
            void* data;
            auto dsize = (VkDeviceSize)length;
            vkMapMemory(vk.device, staging.m, 0, dsize, 0, &data);
                memcpy(data, pixels, length);
            vkUnmapMemory(vk.device, staging.m);
            stbi_image_free(pixels);

            scene.texture = image_create(
                vk,
                {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height),
                    1
                },
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            image_transition(
                vk,
                scene.texture,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );

            auto cb = command_one_off_start(vk);
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
                staging.b,
                scene.texture.i,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &bic
            );
            command_one_off_stop_and_submit(vk, cb);

            image_transition(
                vk,
                scene.texture,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

            vkDestroyBuffer(vk.device, staging.b, nullptr);
            vkFreeMemory(vk.device, staging.m, nullptr);

            VkSamplerCreateInfo sci = {};
            sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sci.magFilter = VK_FILTER_LINEAR;
            sci.minFilter = VK_FILTER_LINEAR;
            sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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

            VkResult r = vkCreateSampler(
                vk.device, &sci, nullptr, &scene.texture.s
            );
            if (r != VK_SUCCESS) LOG(ERROR) << "Could not create image "
                                            << "sampler.";
        }

        /* NOTE(jan): Depth buffer. */
        {
            auto format = format_find_depth(vk);
            scene.depth = image_create(
                vk,
                {swapChain.extent.width, swapChain.extent.height, 1},
                format,
                format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );
            image_transition(
                vk,
                scene.depth,
                format,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            );
        }

        /* NOTE(jan): Framebuffer. */
        {
            swapChain.framebuffers.resize(swapChain.length);
            for (size_t i = 0; i < swapChain.length; i++) {
                VkImageView attachments[] = {
                    swapChain.imageViews[i],
                    scene.depth.v
                };
                VkFramebufferCreateInfo cf = {};
                cf.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                cf.renderPass = pipeline.pass;
                cf.attachmentCount = 2;
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

        /* NOTE(jan): Descriptor pool. */
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

            VkResult r = vkCreateDescriptorPool(
                    vk.device, &dpci, nullptr, &pipeline.descriptorPool
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not create descriptor pool: " << r;
            }
        }

        /* NOTE(jan): Descriptor set. */
        {
            VkDescriptorSetLayout layouts[] = {pipeline.descriptorSetLayout};
            VkDescriptorSetAllocateInfo dsai = {};
            dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            dsai.descriptorPool = pipeline.descriptorPool;
            dsai.descriptorSetCount = 1;
            dsai.pSetLayouts = layouts;

            VkResult r = vkAllocateDescriptorSets(
                    vk.device, &dsai, &pipeline.descriptorSet
            );
            if (r != VK_SUCCESS) {
                LOG(ERROR) << "Could not allocate descriptor set: " << r;
            }

            VkDescriptorBufferInfo dbi = {};
            dbi.buffer = scene.mvp.b;
            dbi.offset = 0;
            dbi.range = sizeof(mvp);

            VkDescriptorImageInfo dii = {};
            dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dii.imageView = scene.texture.v;
            dii.sampler = scene.texture.s;

            VkWriteDescriptorSet wds[2] = {};
            wds[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wds[0].dstSet = pipeline.descriptorSet;
            wds[0].dstBinding = 0;
            wds[0].dstArrayElement = 0;
            wds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wds[0].descriptorCount = 1;
            wds[0].pBufferInfo = &dbi;
            wds[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wds[1].dstSet = pipeline.descriptorSet;
            wds[1].dstBinding = 1;
            wds[1].dstArrayElement = 0;
            wds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wds[1].descriptorCount = 1;
            wds[1].pImageInfo = &dii;

            vkUpdateDescriptorSets(
                    vk.device, 2, wds, 0, nullptr
            );
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
            VkClearValue clear[2] = {};
            clear[0].color = {0.0f, 0.0f, 0.1f, 1.0f};
            clear[1].depthStencil = {1.0f, 0};

            rpbi.clearValueCount = 2;
            rpbi.pClearValues = clear;
            vkCmdBeginRenderPass(
                    commandBuffers[i], &rpbi, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                    commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline.handle
            );
            vkCmdBindDescriptorSets(
                    commandBuffers[i],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline.layout,
                    0, 1,
                    &pipeline.descriptorSet,
                    0, nullptr
            );
            VkBuffer vertex_buffers[] = {scene.vertices.b};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(
                    commandBuffers[i], 0, 1, vertex_buffers, offsets
            );
            vkCmdBindIndexBuffer(
                    commandBuffers[i], scene.indices.b, 0, VK_INDEX_TYPE_UINT16
            );
            vkCmdDrawIndexed(
                commandBuffers[i], static_cast<uint32_t>(indices.size()),
                1, 0, 0, 0
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

            /* NOTE(jan): Calculate MVP. */
            static auto start = std::chrono::high_resolution_clock::now();
            auto now = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<
                    float, std::chrono::seconds::period>(now - start).count();
            mvp.model = glm::rotate(
                    glm::mat4(1.0f),
                    time * glm::radians(90.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f)
            );
            mvp.view = glm::lookAt(
                    glm::vec3(2.0f, 2.0f, 2.0f),
                    glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f)
            );
            mvp.proj = glm::perspective(
                    glm::radians(45.0f),
                    swapChain.extent.width / (float)swapChain.extent.height,
                    0.1f,
                    10.0f
            );
            /* NOTE(jan): Vulkan's y-axis is inverted relative to OpenGL. */
            mvp.proj[1][1] = -1;

            /* NOTE(jan): Copy MVP. */
            void* mvp_dst;
            vkMapMemory(vk.device, scene.mvp.m, 0, sizeof(mvp), 0, &mvp_dst);
                memcpy(mvp_dst, &mvp, sizeof(mvp));
            vkUnmapMemory(vk.device, scene.mvp.m);

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
    vkDestroyImageView(vk.device, scene.depth.v, nullptr);
    vkFreeMemory(vk.device, scene.depth.m, nullptr);
    vkDestroyImage(vk.device, scene.depth.i, nullptr);
    vkDestroySampler(vk.device, scene.texture.s, nullptr);
    vkDestroyImageView(vk.device, scene.texture.v, nullptr);
    vkDestroyImage(vk.device, scene.texture.i, nullptr);
    vkFreeMemory(vk.device, scene.texture.m, nullptr);
    vkFreeMemory(vk.device, scene.indices.m, nullptr);
    vkDestroyBuffer(vk.device, scene.indices.b, nullptr);
    vkFreeMemory(device, scene.vertices.m, nullptr);
    vkDestroyBuffer(device, scene.vertices.b, nullptr);
    vkFreeMemory(device, scene.mvp.m, nullptr);
    vkDestroyBuffer(device, scene.mvp.b, nullptr);
    vkDestroyDescriptorPool(
            vk.device, pipeline.descriptorPool, nullptr
    );
    vkDestroyCommandPool(device, commandPool, nullptr);
    for (const auto& f: swapChain.framebuffers) {
        vkDestroyFramebuffer(device, f, nullptr);
    }
    vkDestroyPipeline(device, pipeline.handle, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
    vkDestroyDescriptorSetLayout(
            vk.device, pipeline.descriptorSetLayout, nullptr);
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
