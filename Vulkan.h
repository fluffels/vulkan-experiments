void
vk_check_success(VkResult r,
                 const char *msg) {
    if (r != VK_SUCCESS) {
        throw(std::runtime_error(msg));
    }
}

struct Vertex {
    glm::vec3 pos;

    static VkVertexInputBindingDescription
    getInputBindingDescription() {
        VkVertexInputBindingDescription i = {};
        i.binding = 0;
        i.stride = sizeof(Vertex);
        i.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return i;
    }

    static std::array<VkVertexInputAttributeDescription, 1>
    getInputAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 1> i = {};
        i[0].binding = 0;
        i[0].location = 0;
        i[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        i[0].offset = offsetof(Vertex, pos);
        return i;
    }
};

struct GridVertex: public Vertex {
    glm::vec3 pos;
    uint8_t type;

    static VkVertexInputBindingDescription
    getInputBindingDescription() {
        VkVertexInputBindingDescription i = {};
        i.binding = 0;
        i.stride = sizeof(GridVertex);
        i.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return i;
    }

    static std::array<VkVertexInputAttributeDescription, 2>
    getInputAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> i = {};
        i[0].binding = 0;
        i[0].location = 0;
        i[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        i[0].offset = offsetof(GridVertex, pos);
        i[1].binding = 0;
        i[1].location = 1;
        i[1].format = VK_FORMAT_R8_UINT;
        i[1].offset = offsetof(GridVertex, type);
        return i;
    }
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

struct Pipeline {
    VkShaderModule vert;
    VkShaderModule frag;
    VkShaderModule geom;
    VkPipelineLayout layout;
    VkPipeline handle;
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
	VkSampleCountFlagBits sampleCount;

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

    bool
    formatHasStencil(VkFormat format) {
        bool result;
        result = (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
        result = result ||(format == VK_FORMAT_D24_UNORM_S8_UINT);
        return result;
    }

    VkDeviceMemory
    allocateMemory(
        VkMemoryRequirements memoryRequirements,
        VkMemoryPropertyFlags memoryProperties
    ) const {
        VkMemoryAllocateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        i.allocationSize = memoryRequirements.size;
        i.memoryTypeIndex = this->findMemoryType(
            memoryRequirements, memoryProperties
        );
        VkDeviceMemory result = {};
        VkResult r = vkAllocateMemory(
            this->device, &i, nullptr, &result
        );
        vk_check_success(r, "could not allocate memory");
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

    void
    transitionImage(const Image& image,
                    VkFormat format,
                    VkImageLayout oldLayout,
                    VkImageLayout newLayout) {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.i;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (this->formatHasStencil(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        VkPipelineStageFlags stage_src;
        VkPipelineStageFlags stage_dst;

        if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) &&
            (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            stage_dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if ((oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) &&
                   (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            stage_src = VK_PIPELINE_STAGE_TRANSFER_BIT;
            stage_dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) &&
                   (newLayout ==
                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            stage_dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) &&
                   (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            stage_dst = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition.");
        }

        auto commandBuffer = this->startCommand();
        vkCmdPipelineBarrier(
            commandBuffer,
            stage_src, stage_dst,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        this->submitCommand(commandBuffer);
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
        result.memory = this->allocateMemory(
            memoryRequirements, memoryProperties
        );
		vkBindBufferMemory(this->device, result.buffer, result.memory, 0);
		return result;
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

    VkDescriptorSetLayout
    createDescriptorSetLayout(
        const std::vector<VkDescriptorSetLayoutBinding>& bindings
    ) {
        VkDescriptorSetLayoutCreateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        i.bindingCount = bindings.size();
        i.pBindings = bindings.data();
        VkDescriptorSetLayout result;
        VkResult code = vkCreateDescriptorSetLayout(
            this->device, &i, nullptr, &result
        );
        if (code != VK_SUCCESS) throw std::runtime_error("Could not create descriptor set layout.");
        return result;
    }

    VkDescriptorPool
    createDescriptorPool(const std::vector<VkDescriptorPoolSize>& size) {
        VkDescriptorPoolCreateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        i.poolSizeCount = size.size();
        i.pPoolSizes = size.data();
        i.maxSets = 1;
        VkDescriptorPool result;
        VkResult code = vkCreateDescriptorPool(this->device, &i, nullptr, &result);
        if (code != VK_SUCCESS) throw std::runtime_error("Could not create descriptor pool.");
        return result;
    }

    VkDescriptorSet
    allocateDescriptorSet(const VkDescriptorPool& pool,
                          const std::vector<VkDescriptorSetLayout>& layouts) {
        VkDescriptorSetAllocateInfo i = {};
        i.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        i.descriptorPool = pool;
        i.descriptorSetCount = layouts.size();
        i.pSetLayouts = layouts.data();
        VkDescriptorSet result;
        VkResult code = vkAllocateDescriptorSets(this->device, &i, &result);
        if (code != VK_SUCCESS) throw std::runtime_error("Could not allocate descriptor set.");
        return result;
    }

    template<typename V>
    Buffer
    createVertexBuffer(const std::vector<V>& vertices) const {
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

    Image
    createImage(VkExtent3D extent,
                 VkSampleCountFlagBits sampleCount,
                 VkFormat format,
                 VkImageUsageFlags usage,
                 VkMemoryPropertyFlags memoryProperties,
                 VkImageAspectFlags aspects) {
        Image result = {};
        {
            VkImageCreateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            i.imageType = VK_IMAGE_TYPE_2D;
            i.extent = extent;
            i.samples = sampleCount;
            i.mipLevels = 1;
            i.arrayLayers = 1;
            i.format = format;
            i.tiling = VK_IMAGE_TILING_OPTIMAL;
            i.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            i.usage = usage;
            i.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VkResult r = vkCreateImage(this->device, &i, nullptr, &result.i);
            if (r != VK_SUCCESS) {
                throw std::runtime_error("Could not create image.");
            }
        }
        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(this->device, result.i, &memoryRequirements);
        result.m = this->allocateMemory(
            memoryRequirements, memoryProperties
        );
        vkBindImageMemory(this->device, result.i, result.m, 0);
        {
            VkImageViewCreateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            i.image = result.i;
            i.viewType = VK_IMAGE_VIEW_TYPE_2D;
            i.format = format;
            i.subresourceRange.layerCount = 1;
            i.subresourceRange.baseArrayLayer = 0;
            i.subresourceRange.aspectMask = aspects;
            i.subresourceRange.baseMipLevel = 0;
            i.subresourceRange.levelCount = 1;
            VkResult r = vkCreateImageView(
                this->device, &i, nullptr, &result.v
            );
            if (r != VK_SUCCESS) {
                throw std::runtime_error("Could not create image view.");
            }
        }
        return result;
    }

    Image
    createTexture(const std::filesystem::path& imagePath,
                  bool tile=false) {
        int width;
        int height;
        int depth;
        auto bytes = readFile(imagePath);
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes.data()), bytes.size(),
            &width, &height, &depth,
            STBI_rgb_alpha
        );
        if (!pixels) {
            throw std::runtime_error("Could not load texture.");
        }
        auto length = width * height * 4;
        auto staging = this->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            length
        );
        void* data;
        auto size = (VkDeviceSize)length;
        vkMapMemory(this->device, staging.memory, 0, size, 0, &data);
            memcpy(data, pixels, length);
        vkUnmapMemory(this->device, staging.memory);
        stbi_image_free(pixels);

        Image result = this->createImage(
            {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                1
            },
			VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        this->transitionImage(
            result,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        {
            auto commandBuffer = this->startCommand();
            VkBufferImageCopy i = {};
            i.bufferOffset = 0;
            i.bufferRowLength = 0;
            i.bufferImageHeight = 0;
            i.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            i.imageSubresource.mipLevel = 0;
            i.imageSubresource.baseArrayLayer = 0;
            i.imageSubresource.layerCount = 1;
            i.imageOffset = {0, 0};
            i.imageExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                1
            };
            vkCmdCopyBufferToImage(
                commandBuffer,
                staging.buffer,
                result.i,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &i
            );
            this->submitCommand(commandBuffer);
        }
        this->transitionImage(
            result,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        vkDestroyBuffer(this->device, staging.buffer, nullptr);
        vkFreeMemory(this->device, staging.memory, nullptr);
        auto addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (tile) addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        {
            VkSamplerCreateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            i.magFilter = VK_FILTER_LINEAR;
            i.minFilter = VK_FILTER_LINEAR;
            i.addressModeU = addressMode;
            i.addressModeV = addressMode;
            i.addressModeW = addressMode;
            i.anisotropyEnable = VK_TRUE;
            i.maxAnisotropy = 16;
            i.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            i.unnormalizedCoordinates = VK_FALSE;
            i.compareEnable = VK_FALSE;
            i.compareOp = VK_COMPARE_OP_ALWAYS;
            i.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            i.mipLodBias = 0.0f;
            i.minLod = 0.0f;
            i.maxLod = 0.0f;
            vk_check_success(
                vkCreateSampler(this->device, &i, nullptr, &result.s),
                "Could not create image sampler."
            );
        }
        return result;
    }

    VkShaderModule
    createShaderModule(const std::filesystem::path& path) {
        auto code = readFile(path);
        VkShaderModule result;
        {
            VkShaderModuleCreateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            i.codeSize = code.size();
            i.pCode = reinterpret_cast<const uint32_t*>(code.data());
            vk_check_success(
                vkCreateShaderModule(this->device, &i, nullptr, &result),
                "Could not create shader module."
            );
        }
        return result;
    }

    template<typename V>
    Pipeline
    createPipeline(const std::filesystem::path& path,
                   VkRenderPass renderPass,
                   VkDescriptorSetLayout descriptorSetLayout) {
        Pipeline result = {};

        std::vector<char> code;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        for (const auto& file : std::filesystem::directory_iterator(path)) {
            VkPipelineShaderStageCreateInfo i = {};
            i.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            i.pName = "main";
            const std::filesystem::path& filePath = file;
            auto filename = filePath.filename();
            if (filename == "vert.spv") {
                result.vert = this->createShaderModule(filePath);
                i.stage = VK_SHADER_STAGE_VERTEX_BIT;
                i.module = result.vert;
            } else if (filename == "frag.spv") {
                result.frag = this->createShaderModule(filePath);
                i.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                i.module = result.frag;
            } else if (filename == "geom.spv") {
                result.geom = this->createShaderModule(filePath);
                i.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                i.module = result.geom;
            }
            if (i.stage) {
                stages.push_back(i);
            }
        }

        auto bindingDescription = V::getInputBindingDescription();
        auto attributeDescriptions = V::getInputAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)this->swap.extent.width;
        viewport.height = (float)this->swap.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = this->swap.extent;

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType =
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.rasterizationSamples = this->sampleCount;
        multisampling.minSampleShading = .2f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
        layoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        layoutCreateInfo.pushConstantRangeCount = 0;
        layoutCreateInfo.pPushConstantRanges = nullptr;
        vk_check_success(
            vkCreatePipelineLayout(
                this->device, &layoutCreateInfo, nullptr, &result.layout
            ),
            "Could not create pipeline layout."
        );

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        VkGraphicsPipelineCreateInfo pipeline = {};
        pipeline.sType =
                VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.stageCount = stages.size();
        pipeline.pStages = stages.data();
        pipeline.pVertexInputState = &vertexInput;
        pipeline.pInputAssemblyState = &inputAssembly;
        pipeline.pViewportState = &viewportState;
        pipeline.pRasterizationState = &rasterizer;
        pipeline.pMultisampleState = &multisampling;
        pipeline.pDepthStencilState = &depthStencil;
        pipeline.pColorBlendState = &colorBlending;
        pipeline.pDynamicState = nullptr;
        pipeline.layout = result.layout;
        pipeline.renderPass = renderPass;
        pipeline.subpass = 0;
        /* NOTE(jan): Used to derive pipelines. */
        pipeline.basePipelineHandle = VK_NULL_HANDLE;
        pipeline.basePipelineIndex = -1;

        vk_check_success(
            vkCreateGraphicsPipelines(
                this->device, VK_NULL_HANDLE, 1, &pipeline, nullptr,
                &result.handle
            ),
            "Could not create graphics pipeline."
        );

        vkDestroyShaderModule(this->device, result.frag, nullptr);
        if (result.geom) {
            vkDestroyShaderModule(this->device, result.geom, nullptr);
        }
        vkDestroyShaderModule(this->device, result.vert, nullptr);

        return result;
    }
};
