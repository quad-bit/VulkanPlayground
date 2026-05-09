#include "tasks/BoundsRenderTask.h"
#include "memory/MemoryManager.h"

Loops::Tasking::BoundsRenderTask::BoundsRenderTask(const GraphicsTaskInfo& info) : GraphicsTask("BoundsRenderTask", info)
{
    CreateAttachments();
    Init();
}

Loops::Tasking::BoundsRenderTask::BoundsRenderTask(const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const VkFormat& colorFormat) : 
    GraphicsTask("BoundsRenderTask", info, colorViews, colorFormat)
{
    Init();
}

void Loops::Tasking::BoundsRenderTask::Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, 
    std::optional<uint64_t> waitValue, const Loops::Bounds* boundArray, size_t numBounds, const glm::mat4& viewProjectionMat)
{
    const uint32_t instanceCount = numBounds;
    // Create transforms from bounds
    {
        for (uint32_t i = 0; i < numBounds; i++)
        {
            const Bounds& bound = boundArray[i];
            glm::vec3 position = (bound.m_max + bound.m_min) / 2.0f;
            glm::vec3 scale = glm::max(glm::vec3(1.0f), glm::abs(bound.m_max - bound.m_min));
            glm::vec3 rotation{ 0.0f };

            auto translationMat = glm::translate(position);
            auto scaleMat = glm::scale(scale);

            glm::mat4 rotXMat = glm::rotate(rotation.x, glm::vec3(1, 0, 0));
            glm::mat4 rotYMat = glm::rotate(rotation.y, glm::vec3(0, 1, 0));
            glm::mat4 rotZMat = glm::rotate(rotation.z, glm::vec3(0, 0, 1));

            auto rotationMat = rotZMat * rotYMat * rotXMat;

            m_transformArray[i] = viewProjectionMat * translationMat * rotationMat * scaleMat;
        }

        ASSERT_MSG(m_transformDataMemoryPointer != nullptr, "not yet mapped");
        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_transformDataMemoryPointer) + m_transformUniformDataSizePerFrame * frameInFlight), m_transformArray, sizeof(glm::mat4) * numBounds);
    }

    // Build Command Buffers
    {
        VkViewport viewport = { 0.0f, static_cast<float>(m_info.m_renderDimensions.m_height), static_cast<float>(m_info.m_renderDimensions.m_width), -static_cast<float>(m_info.m_renderDimensions.m_height), 0.0f, 1.0f };
        VkRect2D   scissor = { {0, 0}, {m_info.m_renderDimensions.m_width, m_info.m_renderDimensions.m_height} };

        Loops::VkUtils::ErrorCheck(vkResetCommandBuffer(m_commandBuffers[frameInFlight], 0));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &beginInfo));

        vkCmdBeginRendering(m_commandBuffers[frameInFlight], &m_renderInfoList[frameInFlight]);
        {
            vkCmdSetViewport(m_commandBuffers[frameInFlight], 0, 1, &viewport);
            vkCmdSetScissor(m_commandBuffers[frameInFlight], 0, 1, &scissor);

            vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            // Bind descriptor sets
            // View set 0
            // Transform set 1
            VkDescriptorSet set {m_transformSets[frameInFlight] };
            VkBindDescriptorSetsInfo bindInfo = {};
            bindInfo.descriptorSetCount = 1;
            bindInfo.firstSet = 0;
            bindInfo.layout = m_pipelineLayout;
            bindInfo.pDescriptorSets = &set;
            bindInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            bindInfo.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO;
            bindInfo.dynamicOffsetCount = 0;
            bindInfo.pDynamicOffsets = nullptr;
            //vkCmdBindDescriptorSets2(m_commandBuffers[frameInFlight], &bindInfo);
            vkCmdBindDescriptorSets(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &set, 0, nullptr);

            // Bind vertex and index buffer
            VkDeviceSize offset{ 0 };
            vkCmdBindVertexBuffers(m_commandBuffers[frameInFlight], 0, 1, &m_vertexBufferWrappers.m_vkVertexBuffer, &offset);
            vkCmdBindIndexBuffer(m_commandBuffers[frameInFlight], m_indexBufferWrappers.m_vkIndexBuffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);

            // Launch draw
            vkCmdDrawIndexed(m_commandBuffers[frameInFlight], m_cubeIndices.size(), instanceCount, 0, 0, 0);
        }
        vkCmdEndRendering(m_commandBuffers[frameInFlight]);

        Loops::VkUtils::ErrorCheck(vkEndCommandBuffer(m_commandBuffers[frameInFlight]));
    }

    Submit(frameInFlight, timelineSem, signalValue, waitValue);
}

void Loops::Tasking::BoundsRenderTask::Init()
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_info.m_queueFamilyIndex;
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    Loops::VkUtils::ErrorCheck(vkCreateCommandPool(m_info.m_device, &createInfo, nullptr, &m_commandPool));

    m_commandBuffers.resize(m_info.m_maxFrameInFlights);
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.commandBufferCount = m_info.m_maxFrameInFlights;
    alloc_info.commandPool = m_commandPool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    Loops::VkUtils::ErrorCheck(vkAllocateCommandBuffers(m_info.m_device, &alloc_info, &m_commandBuffers[0]));

    {
        //set 0 for transform
        m_setLayouts.resize(1);
        {
            VkDescriptorSetLayoutBinding bindings[1]
            {
                //{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
            };

            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 1;
            createInfo.pBindings = &bindings[0];
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_info.m_device, &createInfo, nullptr, &m_setLayouts[0]));
        }

        VkDescriptorPoolSize pool_sizes[1] =
        {
            //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * m_info.m_maxFrameInFlights},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * m_info.m_maxFrameInFlights},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = 0;
        poolInfo.maxSets = 1 * m_info.m_maxFrameInFlights; //transforms 1
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = pool_sizes;
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(m_info.m_device, &poolInfo, nullptr, &m_descriptorPool));
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.pSetLayouts = m_setLayouts.data();
    pipelineLayoutCreateInfo.setLayoutCount = m_setLayouts.size();
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    Loops::VkUtils::ErrorCheck(vkCreatePipelineLayout(m_info.m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

    //Render pass
    VkClearValue clearValues{ VkClearColorValue{0.6033f, 0.6073f, 0.6133f, 1.0f} };
    m_colorInfoList.resize(m_info.m_maxFrameInFlights);

    for (uint32_t i = 0; i < m_info.m_maxFrameInFlights; i++)
    {
        //m_colorInfoList[i].clearValue = clearValues;
        m_colorInfoList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        m_colorInfoList[i].imageView = m_colorAttachmentViews[i];
        m_colorInfoList[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
        m_colorInfoList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
        m_colorInfoList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    }

    for (uint32_t i = 0; i < m_info.m_maxFrameInFlights; i++)
    {
        VkRenderingInfo info{};
        info.colorAttachmentCount = (1);
        info.layerCount = (1);
        info.pColorAttachments = &m_colorInfoList[i];
        info.pDepthAttachment = nullptr;// no depth required
        info.renderArea = VkRect2D{ {0, 0}, {m_info.m_renderDimensions.m_width, m_info.m_renderDimensions.m_height} };
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        m_renderInfoList.push_back(std::move(info));
    }

    {
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_colorFormat;
        //pipelineRenderingCreateInfo.depthAttachmentFormat = m_depthFormat;
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        // Create pipeline
        std::string vertSpvPath = std::string{ SPV_PATH } + "BoundsVert.spv";
        std::string fragSpvPath = std::string{ SPV_PATH } + "BoundsFrag.spv";

        VkPipelineShaderStageCreateInfo vertShaderStage, fragShaderStage;
        std::tie(m_vertexShaderModule, vertShaderStage) = Loops::VkUtils::CreateShaderModule(m_info.m_device, vertSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);
        std::tie(m_fragmentShaderModule, fragShaderStage) = Loops::VkUtils::CreateShaderModule(m_info.m_device, fragSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
        pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Describe the vertex input, i.e. two vertex input attributes in our case:
        VkVertexInputBindingDescription vertexBindings{ 0, sizeof(BoundVertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attributeDescriptions{ 0, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(BoundVertex, BoundVertex::m_pos) };

        pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = &attributeDescriptions;
        pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindings;
        pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;
        pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
        pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
        pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
        pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
        pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.lineWidth = 1.40f;

        VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
        pipelineColorBlendAttachmentState.colorWriteMask = 0xF;
        pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
        pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pipelineColorBlendStateCreateInfo.attachmentCount = 1;
        pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
        pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
        pipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
        pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
        pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_NEVER;
        pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;

        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
        pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pipelineViewportStateCreateInfo.viewportCount = 1;
        pipelineViewportStateCreateInfo.scissorCount = 1;

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
        pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkDynamicState dynamicStates[] =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = 2;
        dynamicStateCreateInfo.pDynamicStates = dynamicStates;

        std::array<VkSpecializationMapEntry, 1> specializationMapEntries;
        specializationMapEntries[0].constantID = 0;
        specializationMapEntries[0].size = sizeof(MAX_BOUNDS);
        specializationMapEntries[0].offset = 0;

        VkSpecializationInfo specializationInfo{};
        specializationInfo.dataSize = sizeof(MAX_BOUNDS);
        specializationInfo.mapEntryCount = specializationMapEntries.size();
        specializationInfo.pData = &MAX_BOUNDS;
        specializationInfo.pMapEntries = specializationMapEntries.data();

        VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {};
        pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStageCreateInfos[0].module = m_vertexShaderModule;
        pipelineShaderStageCreateInfos[0].pName = "main";
        pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        pipelineShaderStageCreateInfos[0].pSpecializationInfo = &specializationInfo;

        pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStageCreateInfos[1].module = m_fragmentShaderModule;
        pipelineShaderStageCreateInfos[1].pName = "main";
        pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
        graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineCreateInfo.layout = m_pipelineLayout;
        graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
        graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
        graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
        graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
        graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
        graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
        graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
        graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
        graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
        graphicsPipelineCreateInfo.stageCount = 2;
        graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

        Loops::VkUtils::ErrorCheck(vkCreateGraphicsPipelines(m_info.m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
            nullptr, &m_pipeline));
    }

    // Transform Uniform
    {
        const uint16_t numUniforms = m_info.m_maxFrameInFlights;

        const size_t dataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_info.m_physicalDevice, sizeof(glm::mat4) * MAX_BOUNDS);
        m_transformUniformDataSizePerFrame = dataSizePerFrame;

        Loops::VkUtils::CreateBufferVma(dataSizePerFrame* numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Loops::Memory::MemoryManager::GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

        vmaMapMemory(Memory::MemoryManager::GetVmaAllocator(), m_transformBuffer.m_vmaAllocation, &m_transformDataMemoryPointer);
        ASSERT_MSG(m_transformDataMemoryPointer != nullptr, "not mapped");

        {
            m_transformSets.resize(numUniforms);

            for (uint16_t i = 0; i < numUniforms; i++)
            {
                VkDescriptorSetAllocateInfo setAllocInfo{};
                setAllocInfo.descriptorPool = m_descriptorPool;
                setAllocInfo.descriptorSetCount = 1;
                setAllocInfo.pSetLayouts = &m_setLayouts[TRANSFORM_SET];
                setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

                Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_info.m_device, &setAllocInfo, &m_transformSets[i]));

                VkDescriptorBufferInfo bufferInfo{ m_transformBuffer.m_vkBuffer, i * dataSizePerFrame, dataSizePerFrame };
                const VkWriteDescriptorSet writes
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_transformSets[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr
                };
                vkUpdateDescriptorSets(m_info.m_device, 1, &writes, 0, nullptr);
            }
        }
    }

    {
        auto CreateBufferAndCopyDataVMA = [this](size_t dataSize, VkBufferUsageFlagBits usage, VkBuffer& buffer, VmaAllocation& vmaAllocation, void* data) -> void
        {
            Loops::VkUtils::CreateBufferVma(dataSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                Loops::Memory::MemoryManager::GetVmaAllocator(), buffer, vmaAllocation);

            // copy data into vertex and index buffer
            auto [stagingBuffer, stagingMemory] = Loops::VkUtils::CreateStagingBuffer(dataSize, m_info.m_physicalDevice, m_info.m_device);

            {
                // map and copy 
                void* pData;
                Loops::VkUtils::ErrorCheck(vkMapMemory(m_info.m_device, stagingMemory, 0, dataSize, 0, &pData));
                memcpy(pData, data, dataSize);
                vkUnmapMemory(m_info.m_device, stagingMemory);
            }

            Loops::VkUtils::CopyFromStagingBuffer(stagingBuffer, buffer, dataSize, m_info.m_device, m_info.m_graphicsQueue, m_info.m_queueFamilyIndex);

            Loops::VkUtils::DestroyBuffer(m_info.m_device, stagingBuffer);
            Loops::VkUtils::FreeMemory(m_info.m_device, stagingMemory);
        };

        const size_t verticiesDataSize = sizeof(glm::vec4) * m_cubeVerticies.size();
        CreateBufferAndCopyDataVMA(verticiesDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBufferWrappers.m_vkVertexBuffer, 
            m_vertexBufferWrappers.m_vmaAllocation, m_cubeVerticies.data());

        const size_t indiciesDataSize = sizeof(uint32_t) * m_cubeIndices.size();
        CreateBufferAndCopyDataVMA(indiciesDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBufferWrappers.m_vkIndexBuffer, 
            m_indexBufferWrappers.m_vmaAllocation, m_cubeIndices.data());
    }
}

Loops::Tasking::BoundsRenderTask::~BoundsRenderTask()
{
    vmaUnmapMemory(Memory::MemoryManager::GetVmaAllocator(), m_transformBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

    {
        vmaDestroyBuffer(Loops::Memory::MemoryManager::GetVmaAllocator(), m_vertexBufferWrappers.m_vkVertexBuffer, m_vertexBufferWrappers.m_vmaAllocation);
        vmaDestroyBuffer(Loops::Memory::MemoryManager::GetVmaAllocator(), m_indexBufferWrappers.m_vkIndexBuffer, m_indexBufferWrappers.m_vmaAllocation);
    }
}

