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

        auto bindingDescription = Vertex::getInputBindingDescription();
        auto attributeDescriptions = Vertex::getInputAttributeDescriptions();
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
