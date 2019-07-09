void
vk_check_success(VkResult r,
                 const char *msg) {
    if (r != VK_SUCCESS) {
        throw(std::runtime_error(msg));
    }
}

struct Vertex {
    glm::vec3 pos;
};

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
    VkCommandPool commandPool;
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

    VkCommandBuffer
    startCommand() const {
        /* TODO(jan): Create a separate command pool for short lived buffers and
         * set VK_COMMAND_POOL_CREATE_TRANSIENT_BIT so the implementation can
         * optimize this. */
        VkCommandBuffer result;
        {
            VkCommandBufferAllocateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            i.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            i.commandPool = this->commandPool;
            i.commandBufferCount = 1;
            vkAllocateCommandBuffers(this->device, &i, &result);
        }
        {
            VkCommandBufferBeginInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            i.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(result, &i);
        }
        return result;
    }

    void
    submitCommand(const VkCommandBuffer& commandBuffer) const {
        vkEndCommandBuffer(commandBuffer);
        {
            VkSubmitInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            i.commandBufferCount = 1;
            i.pCommandBuffers = &commandBuffer;
            vkQueueSubmit(this->queues.graphics.q, 1, &i, VK_NULL_HANDLE);
        }
        vkQueueWaitIdle(this->queues.graphics.q);
        vkFreeCommandBuffers(this->device, this->commandPool, 1, &commandBuffer);
    }

	Buffer
    createDeviceLocalBuffer(
        VkBufferUsageFlags usage,
        VkDeviceSize size,
        void* contents
    ) const {
        auto staging = this->createBuffer(
           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
           size
        );

        void* data;
        vkMapMemory(this->device, staging.memory, 0, size, 0, &data);
        memcpy(data, contents, (size_t)size);
        vkUnmapMemory(this->device, staging.memory);

        auto result = this->createBuffer(
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            size
        );

        VkBufferCopy bufferCopy = {};
        bufferCopy.size = size;
        auto commandBuffer = this->startCommand();
        vkCmdCopyBuffer(commandBuffer, staging.buffer,
                        result.buffer, 1, &bufferCopy);
        this->submitCommand(commandBuffer);

        vkDestroyBuffer(this->device, staging.buffer, nullptr);
        vkFreeMemory(this->device, staging.memory, nullptr);

        return result;
    }

    Buffer
    createVertexBuffer(const std::vector<Vertex>& vertices) const {
        auto result = this->createDeviceLocalBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vector_size(vertices),
            (void*)vertices.data()
        );
        return result;
    }

    Buffer
    createIndexBuffer(const std::vector<uint32_t>& indices) const {
        auto result = this->createDeviceLocalBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            vector_size(indices),
            (void*)indices.data()
        );
        return result;
    }
};

