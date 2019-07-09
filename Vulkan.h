void
vk_check_success(VkResult r,
                 const char *msg) {
    if (r != VK_SUCCESS) {
        throw(std::runtime_error(msg));
    }
}

struct Queue {
    VkQueue q;
    int family_index;
};

struct Queues {
    Queue graphics;
    Queue present;
};

struct Image {
    VkImage i;
    VkImageView v;
    VkDeviceMemory m;
    VkSampler s;
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
    std::vector<VkCommandBuffer> command_buffers;
    VkSemaphore image_available;
    VkSemaphore render_finished;
};

/**
 * The VK structure contains all information related to the Vulkan API,
 * logical and physical devices, and window management. Its members
 * should be invariant across scenes.
 */
class VK {
public:
    VkDevice device;
    VkInstance h;
    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;
    Queues queues;
    SwapChain swap;

	uint32_t
		findMemoryType(const VkMemoryRequirements requirements,
			const VkMemoryPropertyFlags properties) const {
		VkPhysicalDeviceMemoryProperties pdmp;
		vkGetPhysicalDeviceMemoryProperties(physical_device, &pdmp);

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
			throw std::runtime_error("Suitable buffer memory not found");
		}
		return memory_type;
	}

	Buffer
	createBuffer(VkBufferUsageFlags usage,
		         VkMemoryPropertyFlags memoryProperties,
		         VkDeviceSize size) const {
		Buffer result;
		{
			VkBufferCreateInfo i = {};
			i.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			i.size = size;
			i.usage = usage;
			i.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkResult r = vkCreateBuffer(device, &i, nullptr, &result.buffer);
			vk_check_success(r, "could not create buffer");
		}
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(device, result.buffer, &memoryRequirements);
		{
			VkMemoryAllocateInfo i = {};
			i.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			i.allocationSize = memoryRequirements.size;
			i.memoryTypeIndex = this->findMemoryType(
				memoryRequirements, memoryProperties
			);
			VkResult r = vkAllocateMemory(
				this->device, &i, nullptr, &result.memory
			);
			vk_check_success(r, "could not allocate memmory");
		}
		vkBindBufferMemory(this->device, result.buffer, result.memory, 0);
		return result;
	}
};
