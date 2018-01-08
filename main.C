#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "easylogging++.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex;

    bool operator==(const Vertex& other) const {
        return (
            (pos == other.pos) &&
            (color == other.color) &&
            (tex == other.tex)
        );
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(vertex.tex) << 1);
        }
    };
}

struct Queue {
    VkQueue q;
    int family_index;
};

struct Queues {
    Queue graphics;
    Queue present;
};

struct SwapChain {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    VkSurfaceFormatKHR format;
    std::vector<VkPresentModeKHR> modes;
    VkPresentModeKHR mode;
    VkExtent2D extent;
    uint32_t l;
    VkSwapchainKHR h;
    std::vector<Image> images;
    std::vector<VkFramebuffer> frames;
};

/**
 * The VK structure contains all information related to the Vulkan API, logical
 * and physical devices, and window management.
 * Its members should be invariant across scenes.
 */
struct VK {
    VkDevice device;
    VkInstance h;
    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;
    Queues queues;
    SwapChain swap;
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
    Image depth;
};

std::vector<Vertex> vertices;
std::vector<uint32_t> indices;

template<class T> size_t
vector_size(const std::vector<T>& v) {
    size_t result = sizeof(v[0]) * v.size();
    return result;
}

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

const std::vector<const char*> requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int WINDOW_HEIGHT = 600;
const int WINDOW_WIDTH = 800;

INITIALIZE_EASYLOGGINGPP

void
vk_check_success(VkResult r,
                 const char *msg) {
    if (r != VK_SUCCESS) {
        throw(std::runtime_error(msg));
    }
}

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
create_shader_module(VK& vk, const std::vector<char> &code) {
    VkShaderModuleCreateInfo c = {};
    c.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    c.codeSize = code.size();
    c.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    vk_check_success(
        vkCreateShaderModule(vk.device, &c, nullptr, &module),
        "Could not create shader module."
    );
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

    vkQueueSubmit(vk.queues.graphics.q, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk.queues.graphics.q);

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

    LOG(INFO) << "Loading models...";
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;
        auto result = tinyobj::LoadObj(
            &attrib, &shapes, &materials, &err, "chalet.obj"
        );
        if (!result) {
            throw std::runtime_error(err);
        }
        std::unordered_map<Vertex, uint32_t> unique = {};
        for (const auto& shape: shapes) {
            for (const auto& index: shape.mesh.indices) {
                Vertex vertex = {};
                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };
                vertex.tex = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
                vertex.color = {1.0f, 1.0f, 1.0f};
                if (unique.count(vertex) == 0) {
                    unique[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(unique[vertex]);
            }
        }
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
    bool validation_enabled = true;
    std::vector<const char *> layers_requested = {
        "VK_LAYER_LUNARG_standard_validation"
    };
    {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        VkLayerProperties layers_available[count];
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
        LOG(ERROR) << "Layer not present.";
    } else if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        LOG(ERROR) << "Extension not present.";
    } else if (result != VK_SUCCESS) {
        LOG(ERROR) << "Could not instantiate Vulkan.";
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
        cf.pfnCallback = debugCallback;
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
    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(vk.h, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan devices detected.");
    }
    uint32_t queueFamilyCount;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vk.h, &deviceCount, devices.data());
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
                device, vk.surface, &vk.swap.capabilities
        );
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
                device, vk.surface, &formatCount, nullptr
        );
        if (formatCount > 0) {
            vk.swap.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                    device, vk.surface, &formatCount,
                    vk.swap.formats.data()
            );
        }
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, vk.surface, &presentModeCount, nullptr
        );
        if (presentModeCount > 0) {
            vk.swap.modes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, vk.surface, &presentModeCount,
                    vk.swap.modes.data()
            );
        }

        if (requiredExtensionSet.empty() &&
                deviceFeatures.samplerAnisotropy &&
                !vk.swap.formats.empty() &&
                !vk.swap.modes.empty()) {
            vkGetPhysicalDeviceQueueFamilyProperties(
                    device, &queueFamilyCount, nullptr
            );
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(
                    device, &queueFamilyCount, queueFamilies.data()
            );
            vk.queues.graphics.family_index = -1;
            vk.queues.present.family_index = -1;
            int i = 0;
            for (const auto& queueFamily: queueFamilies) {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                        device, i, vk.surface, &presentSupport
                );
                if ((queueFamily.queueCount) & presentSupport) {
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
            vk.physical_device = device;
            vk.physical_device = device;
        }
        break;
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
        VkImage images[vk.swap.l];
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

    /* NOTE(jan): Render passes. */
    {
        VkAttachmentDescription descriptions[2] = {};
        descriptions[0].format = vk.swap.format.format;
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

        vk_check_success(
            vkCreateRenderPass(vk.device, &cf, nullptr, &pipeline.pass),
            "Could not create render pass."
        );
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
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo dslci = {};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 2;
        dslci.pBindings = bindings;

        vk_check_success(
            vkCreateDescriptorSetLayout(
                vk.device, &dslci, nullptr, &pipeline.descriptorSetLayout
            ),
            "Could not create descriptor set layout."
        );
    }

    /* NOTE(jan): Create pipeline. */
    {
        auto code = readFile("shaders/triangle/vert.spv");
        pipeline.vertModule = create_shader_module(vk, code);
        code = readFile("shaders/triangle/frag.spv");
        pipeline.fragModule = create_shader_module(vk, code);

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
        viewport.width = (float)vk.swap.extent.width;
        viewport.height = (float)vk.swap.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = vk.swap.extent;

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
        vk_check_success(
            vkCreatePipelineLayout(
                vk.device, &layoutCreateInfo, nullptr, &pipeline.layout
            ),
            "Could not create pipeline layout."
        );

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

        vk_check_success(
            vkCreateGraphicsPipelines(
                vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                &pipeline.handle
            ),
            "Could not create graphics pipeline."
        );

        vkDestroyShaderModule(vk.device, pipeline.fragModule, nullptr);
        vkDestroyShaderModule(vk.device, pipeline.vertModule, nullptr);
    }

    /* NOTE(jan): Command pool creation. */
    {
        VkCommandPoolCreateInfo cf = {};
        cf.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cf.queueFamilyIndex = vk.queues.graphics.family_index;
        cf.flags = 0;
        vk_check_success(
            vkCreateCommandPool(vk.device, &cf, nullptr, &commandPool),
            "Could not create command pool."
        );
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
        VkDeviceSize size = sizeof(scene.mvp);
        scene.uniforms = buffer_create(
            vk,
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
            "chalet.jpg", &width, &height, &depth, STBI_rgb_alpha
        );
        if (!pixels) {
            throw std::runtime_error("Could not load texture.");
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

        vk_check_success(
            vkCreateSampler(vk.device, &sci, nullptr, &scene.texture.s),
            "Could not create image sampler."
        );
    }

    /* NOTE(jan): Depth buffer. */
    {
        auto format = format_find_depth(vk);
        scene.depth = image_create(
            vk,
            {vk.swap.extent.width, vk.swap.extent.height, 1},
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
        vk.swap.frames.resize(vk.swap.l);
        for (size_t i = 0; i < vk.swap.l; i++) {
            VkImageView attachments[] = {
                vk.swap.images[i].v,
                scene.depth.v
            };
            VkFramebufferCreateInfo cf = {};
            cf.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            cf.renderPass = pipeline.pass;
            cf.attachmentCount = 2;
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
                vk.device, &dpci, nullptr, &pipeline.descriptorPool
            ),
            "Could not create descriptor pool."
        );
    }

    /* NOTE(jan): Descriptor set. */
    {
        VkDescriptorSetLayout layouts[] = {pipeline.descriptorSetLayout};
        VkDescriptorSetAllocateInfo dsai = {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = pipeline.descriptorPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = layouts;

        vk_check_success(
            vkAllocateDescriptorSets(
                vk.device, &dsai, &pipeline.descriptorSet
            ),
            "Could not allocate descriptor set."
        );

        VkDescriptorBufferInfo dbi = {};
        dbi.buffer = scene.uniforms.b;
        dbi.offset = 0;
        dbi.range = sizeof(scene.mvp);

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

        vkUpdateDescriptorSets(vk.device, 2, wds, 0, nullptr);
    }

    /* NOTE(jan): Command buffer creation. */
    {
        commandBuffers.resize(vk.swap.l);
        VkCommandBufferAllocateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        i.commandPool = commandPool;
        i.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        i.commandBufferCount = (uint32_t)commandBuffers.size();
        vk_check_success(
            vkAllocateCommandBuffers(vk.device, &i, commandBuffers.data()),
            "Could not allocate command buffers."
        );
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
        rpbi.framebuffer = vk.swap.frames[i];
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent = vk.swap.extent;
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
            commandBuffers[i], scene.indices.b, 0, VK_INDEX_TYPE_UINT32
        );
        vkCmdDrawIndexed(
            commandBuffers[i], static_cast<uint32_t>(indices.size()),
            1, 0, 0, 0
        );
        vkCmdEndRenderPass(commandBuffers[i]);
        vk_check_success(
            vkEndCommandBuffer(commandBuffers[i]),
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
            vk.device, &sci, nullptr, &imageAvailable
        );
        success = result == VK_SUCCESS;
        result = vkCreateSemaphore(
            vk.device, &sci, nullptr, &renderFinished
        );
        success = success && (result == VK_SUCCESS);
        if (!success) {
            throw std::runtime_error("Could not create semaphores.");
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
        scene.mvp.model = glm::rotate(
            glm::mat4(1.0f),
            time * glm::radians(90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        scene.mvp.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        scene.mvp.proj = glm::perspective(
            glm::radians(45.0f),
            vk.swap.extent.width / (float)vk.swap.extent.height,
            0.1f,
            10.0f
        );
        /* NOTE(jan): Vulkan's y-axis is inverted relative to OpenGL. */
        scene.mvp.proj[1][1] *= -1;

        /* NOTE(jan): Copy MVP. */
        void* mvp_dst;
        size_t s = sizeof(scene.mvp);
        vkMapMemory(vk.device, scene.uniforms.m, 0, s, 0, &mvp_dst);
            memcpy(mvp_dst, &scene.mvp, s);
        vkUnmapMemory(vk.device, scene.uniforms.m);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            vk.device, vk.swap.h,
            std::numeric_limits<uint64_t>::max(),
            imageAvailable, VK_NULL_HANDLE, &imageIndex
        );

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailable};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = {renderFinished};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        vk_check_success(
            vkQueueSubmit(vk.queues.graphics.q, 1, &submitInfo, VK_NULL_HANDLE),
            "Could not submit to graphics queue."
        );

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
    if (vkDestroyCallback != nullptr) {
        vkDestroyCallback(vk.h, callback_debug, nullptr);
    }
    vkDestroySemaphore(vk.device, renderFinished, nullptr);
    vkDestroySemaphore(vk.device, imageAvailable, nullptr);
    vkDestroyImageView(vk.device, scene.depth.v, nullptr);
    vkFreeMemory(vk.device, scene.depth.m, nullptr);
    vkDestroyImage(vk.device, scene.depth.i, nullptr);
    vkDestroySampler(vk.device, scene.texture.s, nullptr);
    vkDestroyImageView(vk.device, scene.texture.v, nullptr);
    vkDestroyImage(vk.device, scene.texture.i, nullptr);
    vkFreeMemory(vk.device, scene.texture.m, nullptr);
    vkFreeMemory(vk.device, scene.indices.m, nullptr);
    vkDestroyBuffer(vk.device, scene.indices.b, nullptr);
    vkFreeMemory(vk.device, scene.vertices.m, nullptr);
    vkDestroyBuffer(vk.device, scene.vertices.b, nullptr);
    vkFreeMemory(vk.device, scene.uniforms.m, nullptr);
    vkDestroyBuffer(vk.device, scene.uniforms.b, nullptr);
    vkDestroyDescriptorPool(
        vk.device, pipeline.descriptorPool, nullptr
    );
    vkDestroyCommandPool(vk.device, commandPool, nullptr);
    for (const auto& f: vk.swap.frames) {
        vkDestroyFramebuffer(vk.device, f, nullptr);
    }
    vkDestroyPipeline(vk.device, pipeline.handle, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline.layout, nullptr);
    vkDestroyDescriptorSetLayout(
        vk.device, pipeline.descriptorSetLayout, nullptr
    );
    vkDestroyRenderPass(vk.device, pipeline.pass, nullptr);
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
