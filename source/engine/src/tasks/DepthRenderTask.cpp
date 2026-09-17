#include "tasks/DepthRenderTask.h"

void Loops::Tasking::DepthRenderTask::Init(bool allocateCommandBuffers,
    const VkClearDepthStencilValue& depthStencilClearValue,
    VkCullModeFlags cullMode)
{
    if (allocateCommandBuffers)
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = m_vulkanContext->m_graphicsQueueFamilyIndex;
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        Loops::VkUtils::ErrorCheck(vkCreateCommandPool(m_vulkanContext->m_logicalDevice, &createInfo, nullptr, &m_commandPool));

        m_commandBuffers.resize(m_vulkanContext->m_maxFrameInFlights);
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.commandBufferCount = m_vulkanContext->m_maxFrameInFlights;
        alloc_info.commandPool = m_commandPool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        Loops::VkUtils::ErrorCheck(vkAllocateCommandBuffers(m_vulkanContext->m_logicalDevice, &alloc_info, &m_commandBuffers[0]));
    }

    {
        // descriptor pool not required as scene and transform set are created in SceneManager
    }

    std::array<VkPushConstantRange, 1> range
    {
        VkPushConstantRange
        {
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(PushConsts)
        }
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.pPushConstantRanges = range.data();
    pipelineLayoutCreateInfo.pSetLayouts = m_customLayout.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)range.size();
    pipelineLayoutCreateInfo.setLayoutCount = m_customLayout.size();
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    Loops::VkUtils::ErrorCheck(vkCreatePipelineLayout(m_vulkanContext->m_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

    // pipeline
    {
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.colorAttachmentCount = 0;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = nullptr;
        pipelineRenderingCreateInfo.depthAttachmentFormat = m_depthFormat;
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        // Create pipeline
        std::string vertSpvPath = std::string{ SPV_PATH } + "DepthVert.spv";
        std::string fragSpvPath = std::string{ SPV_PATH } + "DepthFrag.spv";

        VkPipelineShaderStageCreateInfo vertShaderStage, fragShaderStage;
        std::tie(m_vertexShaderModule, vertShaderStage) = Loops::VkUtils::CreateShaderModule(m_vulkanContext->m_logicalDevice, vertSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);
        std::tie(m_fragmentShaderModule, fragShaderStage) = Loops::VkUtils::CreateShaderModule(m_vulkanContext->m_logicalDevice, fragSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
        pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Describe the vertex input, i.e. two vertex input attributes in our case:
        VkVertexInputBindingDescription vertexBindings{ 0, sizeof(Loops::Vertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX };
        std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions
        {
            VkVertexInputAttributeDescription{0, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Loops::Vertex, Loops::Vertex::m_position)}
        };

        pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindings;
        pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
        pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
        pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
        pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineRasterizationStateCreateInfo.cullMode = cullMode;
        pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

        VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
        pipelineColorBlendAttachmentState.colorWriteMask = 0xF;
        pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
        pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pipelineColorBlendStateCreateInfo.attachmentCount = 1;
        pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
        pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
        pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
        pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        pipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
        pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        pipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
        pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_GREATER;
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

        VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {};
        pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStageCreateInfos[0].module = m_vertexShaderModule;
        pipelineShaderStageCreateInfos[0].pName = "main";
        pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        pipelineShaderStageCreateInfos[0].pSpecializationInfo = nullptr;

        pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineShaderStageCreateInfos[1].module = m_fragmentShaderModule;
        pipelineShaderStageCreateInfos[1].pName = "main";
        pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipelineShaderStageCreateInfos[1].pSpecializationInfo = nullptr;

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

        Loops::VkUtils::ErrorCheck(vkCreateGraphicsPipelines(m_vulkanContext->m_logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
            nullptr, &m_pipeline));
    }

    if (!m_ownAttachments)
    {
        //Render pass

        VkClearValue clearValuesDepth{};
        clearValuesDepth.depthStencil = depthStencilClearValue;
        m_depthInfoList.resize(m_vulkanContext->m_maxFrameInFlights);

        for (uint32_t i = 0; i < m_depthAttachmentViews.size(); i++)
        {
            m_depthInfoList[i].clearValue = clearValuesDepth;
            m_depthInfoList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            m_depthInfoList[i].imageView = m_depthAttachmentViews[i];
            m_depthInfoList[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR ;
            m_depthInfoList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            m_depthInfoList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        }

        for (uint32_t i = 0; i < m_vulkanContext->m_maxFrameInFlights; i++)
        {
            VkRenderingInfo info{};
            info.colorAttachmentCount = 0;
            info.layerCount = 1;
            info.pColorAttachments = nullptr;
            info.pDepthAttachment = &m_depthInfoList[i];
            info.renderArea = VkRect2D{ {0, 0}, {m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height} };
            info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            m_renderInfoList.push_back(std::move(info));
        }
    }
}

Loops::Tasking::DepthRenderTask::DepthRenderTask(const VkUtils::VulkanContext * const vulkanContext,
    const VkDescriptorSetLayout& transformSetLayout,
    const VkDescriptorSetLayout& sceneSetLayout,
    const std::vector<VkDescriptorSet>& sceneSet,
    const std::vector<VkDescriptorSet>& transformSet,
    const VkFormat& depthFormat, uint32_t numTargets,
    const VkClearDepthStencilValue depthStencilClearValue,
    std::vector<std::pair<Loops::EFFECT_TYPE, Loops::TECHNIQUE_TYPE>> targetPasses,
    bool allocateCommandBuffers, VkCullModeFlags cullMode):
    GraphicsTask("DepthRenderTask", vulkanContext, numTargets, depthFormat, depthStencilClearValue),
        m_customLayout{ sceneSetLayout, transformSetLayout }, m_sceneSet(sceneSet),
        m_transformSet(transformSet), m_targetPasses(targetPasses)
{
    Init(allocateCommandBuffers, depthStencilClearValue, cullMode);
}

void Loops::Tasking::DepthRenderTask::Update(const uint32_t& frameInFlight,
    const VkSemaphore& timelineSem,
    std::optional<uint64_t> waitValue,
    const Loops::RenderData& renderData, const Loops::SceneManager* sceneManager,
    const std::unordered_map<uint32_t, Loops::Material>& materials,
    const VkDescriptorSet& transformSet, std::mutex& mutex,
    std::atomic_uint64_t& signalAtomic)
{
    VkViewport viewport = { 0.0f, static_cast<float>(m_vulkanContext->m_renderDimensions.m_height), static_cast<float>(m_vulkanContext->m_renderDimensions.m_width), -static_cast<float>(m_vulkanContext->m_renderDimensions.m_height), 0.0f, 1.0f };
    VkRect2D scissor = { {0, 0}, {m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height} };
    VkCommandBuffer& commandBuffer = m_commandBuffers[frameInFlight];

    auto RenderCommands = [this, &viewport, &scissor,
        &renderData, &sceneManager, &commandBuffer](
            uint32_t frameInFlight,
            const Loops::EFFECT_TYPE& targetEffect,
            const Loops::TECHNIQUE_TYPE& targetTech)
        {
            
            vkCmdBeginRendering(commandBuffer, &m_renderInfoList[frameInFlight]);
            {
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                int boundVertexBuffer = -1, boundIndexBuffer = -1;
                auto techIt = renderData.m_drawablesPerMaterial.find(targetEffect);
                if (techIt != renderData.m_drawablesPerMaterial.end())
                {
                    // Bind descriptor sets
                    // Scene set 0
                    // Transform set 1
                    VkDescriptorSet set[2]
                    {
                        m_sceneSet[frameInFlight],
                        m_transformSet[frameInFlight],
                    };

                    VkBindDescriptorSetsInfo bindInfo = {};
                    bindInfo.descriptorSetCount = 2;
                    bindInfo.firstSet = 0;
                    bindInfo.layout = m_pipelineLayout;
                    bindInfo.pDescriptorSets = set;
                    bindInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO;
                    bindInfo.dynamicOffsetCount = 0;
                    bindInfo.pDynamicOffsets = nullptr;
                    vkCmdBindDescriptorSets2(commandBuffer, &bindInfo);

                    auto RecordDraw = [&renderData, &boundIndexBuffer,
                        &boundVertexBuffer, frameInFlight,
                        &sceneManager, this,
                        &commandBuffer](const std::vector<uint32_t>& drawableIndicies)
                        {
                            for (const auto& drawableIndex : drawableIndicies)
                            {
                                const Loops::Drawable& drawable = renderData.m_drawables[drawableIndex];

                                // Bind vertex and index buffer
                                if (boundVertexBuffer != drawable.m_vertexBufferId || boundIndexBuffer != drawable.m_indexBufferId)
                                {
                                    boundVertexBuffer = drawable.m_vertexBufferId;
                                    boundIndexBuffer = drawable.m_indexBufferId;
                                    auto& vertexBuffer = sceneManager->GetVertexBuffer(drawable.m_vertexBufferId);
                                    auto& indexBuffer = sceneManager->GetIndexBuffer(drawable.m_indexBufferId);

                                    VkDeviceSize offset{ 0 };
                                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
                                    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
                                }

                                // Push Constant 
                                uint32_t matrixIndex = drawable.m_matrixIndex;

                                // Launch draw
                                for (uint32_t i = 0; i < drawable.m_numOfViews; i++)
                                {
                                    const Loops::MeshView& meshView = renderData.m_meshViews[drawable.m_viewStartIndex + i];
                                    PushConsts pushConsts{ (int)matrixIndex };
                                    VkPushConstantsInfo info{};
                                    info.layout = m_pipelineLayout;
                                    info.offset = 0;
                                    info.pValues = &pushConsts;
                                    info.size = sizeof(PushConsts);
                                    info.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                                    info.sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO;
                                    vkCmdPushConstants2(commandBuffer, &info);

                                    uint32_t numIndicies = meshView.m_indexCount;
                                    uint32_t firstIndex = meshView.m_firstIndex;
                                    vkCmdDrawIndexed(commandBuffer, numIndicies, 1, firstIndex, 0, 0);
                                }
                            }
                        };

                    auto pbrIt = techIt->second.find(targetTech);
                    if (pbrIt != techIt->second.end())
                    {
                        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                        const std::vector<uint32_t>& drawableIndicies = pbrIt->second;
                        RecordDraw(drawableIndicies);
                    }
                }
            }
            vkCmdEndRendering(commandBuffer);

        };

    Loops::VkUtils::ErrorCheck(vkResetCommandBuffer(commandBuffer, 0));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    {
        VkImageMemoryBarrier2 imageBarrier{};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        // Define the synchronization stages
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // Source: Transfer/Copy operation
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; // Destination: Shader reading (e.g., Fragment shader)

        // Define the access masks (caches to flush/invalidate)
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT; // Flush transfer writes
        imageBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Invalidate shader reads

        // Layout transitions
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Ownership queue family transfers (ignored if not changing queues)
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // Define the subresource range (which parts of the image to transition)
        imageBarrier.image = std::get<TaskOwnedResource>(m_taskResource).m_depthTargets[frameInFlight].m_vkImage;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.layerCount = 1;

        // Package the barrier into dependency info
        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = 0;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;

        // Record the pipeline barrier
        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }

    for (const auto& targetPass : m_targetPasses)
    {
        RenderCommands(frameInFlight, targetPass.first, targetPass.second);
    }

    {
        VkImageMemoryBarrier2 imageBarrier{};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        // Define the synchronization stages
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // Source: Transfer/Copy operation
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; // Destination: Shader reading (e.g., Fragment shader)

        // Define the access masks (caches to flush/invalidate)
        imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT; // Flush transfer writes
        imageBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Invalidate shader reads

        // Layout transitions
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Ownership queue family transfers (ignored if not changing queues)
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // Define the subresource range (which parts of the image to transition)
        imageBarrier.image = std::get<TaskOwnedResource>(m_taskResource).m_depthTargets[frameInFlight].m_vkImage;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.layerCount = 1;

        // Package the barrier into dependency info
        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = 0;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;

        // Record the pipeline barrier
        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }
    Loops::VkUtils::ErrorCheck(vkEndCommandBuffer(commandBuffer));

    {
        std::lock_guard lock(mutex);
        const uint64_t signalValue = ++signalAtomic;
        Submit(frameInFlight, timelineSem, signalValue, waitValue);
    }
}

void Loops::Tasking::DepthRenderTask::Update(VkCommandBuffer& commandBuffer,
    const uint32_t& frameInFlight, const Loops::RenderData& renderData,
    const Loops::SceneManager& sceneManager)
{
    VkViewport viewport = { 0.0f, static_cast<float>(m_vulkanContext->m_renderDimensions.m_height), static_cast<float>(m_vulkanContext->m_renderDimensions.m_width), -static_cast<float>(m_vulkanContext->m_renderDimensions.m_height), 0.0f, 1.0f };
    VkRect2D scissor = { {0, 0}, {m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height} };

    auto RenderCommands = [this, &viewport, &scissor,
        &renderData, &sceneManager, &commandBuffer](
            uint32_t frameInFlight,
            const Loops::EFFECT_TYPE& targetEffect,
            const Loops::TECHNIQUE_TYPE& targetTech)
        {
            vkCmdBeginRendering(commandBuffer, &m_renderInfoList[frameInFlight]);
            {
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                int boundVertexBuffer = -1, boundIndexBuffer = -1;
                auto techIt = renderData.m_drawablesPerMaterial.find(targetEffect);
                if (techIt != renderData.m_drawablesPerMaterial.end())
                {
                    // Bind descriptor sets
                    // Scene set 0
                    // Transform set 1
                    VkDescriptorSet set[2]
                    {
                        m_sceneSet[frameInFlight],
                        m_transformSet[frameInFlight],
                    };

                    VkBindDescriptorSetsInfo bindInfo = {};
                    bindInfo.descriptorSetCount = 2;
                    bindInfo.firstSet = 0;
                    bindInfo.layout = m_pipelineLayout;
                    bindInfo.pDescriptorSets = set;
                    bindInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO;
                    bindInfo.dynamicOffsetCount = 0;
                    bindInfo.pDynamicOffsets = nullptr;
                    vkCmdBindDescriptorSets2(commandBuffer, &bindInfo);

                    auto RecordDraw = [&renderData, &boundIndexBuffer,
                        &boundVertexBuffer, frameInFlight,
                        &sceneManager, this,
                        &commandBuffer](const std::vector<uint32_t>& drawableIndicies)
                        {
                            for (const auto& drawableIndex : drawableIndicies)
                            {
                                const Loops::Drawable& drawable = renderData.m_drawables[drawableIndex];

                                // Bind vertex and index buffer
                                if (boundVertexBuffer != drawable.m_vertexBufferId || boundIndexBuffer != drawable.m_indexBufferId)
                                {
                                    boundVertexBuffer = drawable.m_vertexBufferId;
                                    boundIndexBuffer = drawable.m_indexBufferId;
                                    auto& vertexBuffer = sceneManager.GetVertexBuffer(drawable.m_vertexBufferId);
                                    auto& indexBuffer = sceneManager.GetIndexBuffer(drawable.m_indexBufferId);

                                    VkDeviceSize offset{ 0 };
                                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
                                    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
                                }

                                // Push Constant 
                                uint32_t matrixIndex = drawable.m_matrixIndex;

                                // Launch draw
                                for (uint32_t i = 0; i < drawable.m_numOfViews; i++)
                                {
                                    const Loops::MeshView& meshView = renderData.m_meshViews[drawable.m_viewStartIndex + i];
                                    PushConsts pushConsts{ (int)matrixIndex};
                                    VkPushConstantsInfo info{};
                                    info.layout = m_pipelineLayout;
                                    info.offset = 0;
                                    info.pValues = &pushConsts;
                                    info.size = sizeof(PushConsts);
                                    info.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                                    info.sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO;
                                    vkCmdPushConstants2(commandBuffer, &info);

                                    uint32_t numIndicies = meshView.m_indexCount;
                                    uint32_t firstIndex = meshView.m_firstIndex;
                                    vkCmdDrawIndexed(commandBuffer, numIndicies, 1, firstIndex, 0, 0);
                                }
                            }
                        };

                    auto pbrIt = techIt->second.find(targetTech);
                    if (pbrIt != techIt->second.end())
                    {
                        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                        const std::vector<uint32_t>& drawableIndicies = pbrIt->second;
                        RecordDraw(drawableIndicies);
                    }
                }
            }
            vkCmdEndRendering(commandBuffer);
        };

    for (const auto& targetPass : m_targetPasses)
    {
        RenderCommands(frameInFlight, targetPass.first, targetPass.second);
    }
}

std::vector<VkImageView> Loops::Tasking::DepthRenderTask::GetDepthTargetViews() const
{
    std::vector<VkImageView> views;

    const TaskOwnedResource& resource = std::get<TaskOwnedResource>(m_taskResource);
    for (auto& target : resource.m_depthTargets)
    {
        views.push_back( target.m_vkImageView);
    }

    return views;
}

Loops::Tasking::DepthRenderTask::~DepthRenderTask()
{
}
