#include "WireFrameTask.h"
#include "Mesh.h"
#include "SceneNode.h"
#include <memory>

void Common::WireFrameTask::BuildCommandBuffers(const uint32_t& frameInFlight, bool changeImageLayout)
{
    VkViewport viewport = { 0.0f, static_cast<float>(m_screenHeight), static_cast<float>(m_screenWidth), -static_cast<float>(m_screenHeight), 0.0f, 1.0f };
    VkRect2D   scissor = { {0, 0}, {m_screenWidth, m_screenHeight} };

    ErrorCheck(vkResetCommandBuffer(m_commandBuffers[frameInFlight], 0));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &beginInfo));

    if (changeImageLayout)
    {
        //VkImageMemoryBarrier image_barrier2{};
        //image_barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        //image_barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        //image_barrier2.dstAccessMask = 0;
        //image_barrier2.image = m_colorAttachments[frameInFlight];
        //image_barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        //image_barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        //image_barrier2.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //// The semaphore takes care of srcStageMask.
        //vkCmdPipelineBarrier(m_commandBuffers[frameInFlight],
        //    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //    0, 0, nullptr, 0, nullptr, 1, &image_barrier2);
    }

    vkCmdBeginRendering(m_commandBuffers[frameInFlight], &m_renderInfoList[frameInFlight]);
    {
        vkCmdSetViewport(m_commandBuffers[frameInFlight], 0, 1, &viewport);
        vkCmdSetScissor(m_commandBuffers[frameInFlight], 0, 1, &scissor);

        //vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        //std::array<VkBuffer, 2> vertexBuffs{ m_vertexBuffers[frameInFlight], m_uvBuffer };
        //std::array<VkDeviceSize, 2> offsets{ 0, 0 };
        //vkCmdBindVertexBuffers(m_commandBuffers[frameInFlight], 0, 2, vertexBuffs.data(), offsets.data());

        //vkCmdBindIndexBuffer(m_commandBuffers[frameInFlight], m_indexBuffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);

        //std::array<VkDescriptorSet, 3> sets{ m_viewSet[frameInFlight], m_transformSet[frameInFlight], m_samplerSet[frameInFlight] };
        //vkCmdBindDescriptorSets(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 3, &sets[0], 0, nullptr);

        //vkCmdDrawIndexed(m_commandBuffers[frameInFlight], m_numIndicies, 1, 0, 0, 0);
    }
    vkCmdEndRendering(m_commandBuffers[frameInFlight]);

    ErrorCheck(vkEndCommandBuffer(m_commandBuffers[frameInFlight]));
}

Common::WireFrameTask::WireFrameTask(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue,
    uint32_t queueFamilyIndex, uint32_t maxFrameInFlight, uint32_t screenWidth, uint32_t screenHeight, const VkFormat& depthFormat,
    const std::vector<VkImageView>& colorViews, const std::vector<VkImageView>& depthViews, std::shared_ptr<Camera> pCamera) :
    m_device(device), m_physicalDevice(physicalDevice), m_graphicsQueue(graphicsQueue), m_pCamera(pCamera), m_colorAttachmentViews(colorViews),
    m_depthAttachmentViews(depthViews), m_screenWidth(screenWidth), m_screenHeight(screenHeight), m_maxFrameInFlights(maxFrameInFlight)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = queueFamilyIndex;
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    ErrorCheck(vkCreateCommandPool(device, &createInfo, nullptr, &m_commandPool));

    m_commandBuffers.resize(maxFrameInFlight);
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.commandBufferCount = maxFrameInFlight;
    alloc_info.commandPool = m_commandPool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    ErrorCheck(vkAllocateCommandBuffers(device, &alloc_info, &m_commandBuffers[0]));

    {
        VkDescriptorSetLayoutBinding bindings[1]
        {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
        };

        VkDescriptorSetLayoutCreateInfo createInfo{};
        createInfo.bindingCount = 1;
        createInfo.pBindings = &bindings[0];
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ErrorCheck(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_viewSetLayout));

        VkDescriptorPoolSize pool_sizes[1] =
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * maxFrameInFlight},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = 0;
        poolInfo.maxSets = 1 * maxFrameInFlight;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = pool_sizes;
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ErrorCheck(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool));
    }

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(glm::mat4);
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.pPushConstantRanges = &range;
    pipelineLayoutCreateInfo.pSetLayouts = &m_viewSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    ErrorCheck(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

    //// Render pass attachments
    //m_colorAttachmentViews.resize(maxFrameInFlight);
    //for (int i = 0; i < maxFrameInFlight; ++i)
    //{
    //    auto [image, memory] = CreateImage(device, physicalDevice, screenWidth, screenHeight, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    //    m_colorAttachments.push_back(std::move(image));
    //    m_colorAttachmentMemory.push_back(std::move(memory));

    //    VkImageViewCreateInfo createInfo{};
    //    createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
    //    createInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    //    createInfo.image = m_colorAttachments[i];
    //    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    //    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //    createInfo.subresourceRange.baseArrayLayer = 0;
    //    createInfo.subresourceRange.baseMipLevel = 0;
    //    createInfo.subresourceRange.layerCount = 1;
    //    createInfo.subresourceRange.levelCount = 1;
    //    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    //    ErrorCheck(vkCreateImageView(device, &createInfo, nullptr, &m_colorAttachmentViews[i]));
    //    ChangeImageLayout(m_device, m_colorAttachments, m_graphicsQueue, queueFamilyIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //}

    ////Depth
    //{
    //    std::tie(m_depthAttachment, m_depthAttachmentMemory) = CreateImage(device, physicalDevice, screenWidth, screenHeight, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    //    VkImageViewCreateInfo createInfo{};
    //    createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
    //    createInfo.format = depthFormat;
    //    createInfo.image = m_depthAttachment;
    //    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    //    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    //    createInfo.subresourceRange.baseArrayLayer = 0;
    //    createInfo.subresourceRange.baseMipLevel = 0;
    //    createInfo.subresourceRange.layerCount = 1;
    //    createInfo.subresourceRange.levelCount = 1;
    //    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    //    ErrorCheck(vkCreateImageView(device, &createInfo, nullptr, &m_depthAttachmentViews));
    //}

    //Render pass
    VkClearValue clearValues{ VkClearColorValue{0.6033f, 0.6073f, 0.6133f, 1.0f} };
    colorInfoList.resize(maxFrameInFlight);

    VkClearValue clearValuesDepth;
    clearValuesDepth.depthStencil = { VkClearDepthStencilValue{ 1.0f, 0u } };
    depthInfoList.resize(maxFrameInFlight);

    for (uint32_t i = 0; i < maxFrameInFlight; i++)
    {
        colorInfoList[i].clearValue = clearValues;
        colorInfoList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorInfoList[i].imageView = m_colorAttachmentViews[i];
        colorInfoList[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorInfoList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
        colorInfoList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        depthInfoList[i].clearValue = clearValuesDepth;
        depthInfoList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthInfoList[i].imageView = m_depthAttachmentViews[i];
        depthInfoList[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthInfoList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
        depthInfoList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    }

    for (uint32_t i = 0; i < maxFrameInFlight; i++)
    {
        VkRenderingInfo info{};
        info.colorAttachmentCount = (1);
        info.layerCount = (1);
        info.pColorAttachments = &colorInfoList[i];
        info.pDepthAttachment = &depthInfoList[i];
        info.renderArea = VkRect2D{ {0, 0}, {(uint32_t)screenWidth, (uint32_t)screenHeight} };
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        m_renderInfoList.push_back(std::move(info));
    }

    VkFormat attachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &attachmentFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    // Create pipeline
    std::string vertSpvPath = std::string{ SPV_PATH } + "UnlitColorVert.spv";
    std::string fragSpvPath = std::string{ SPV_PATH } + "UnlitColorFrag.spv";

    VkPipelineShaderStageCreateInfo vertShaderStage, fragShaderStage;
    std::tie(m_vertexShaderModule, vertShaderStage) = CreateShaderModule(device, vertSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);
    std::tie(m_fragmentShaderModule, fragShaderStage) = CreateShaderModule(device, fragSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Describe the vertex input, i.e. two vertex input attributes in our case:
    VkVertexInputBindingDescription vertexBindings{ 0, sizeof(Common::Vertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attributeDescriptions{ 0, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Common::Vertex, Common::Vertex::m_position) };

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
    /*pipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_GREATER;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;
    */

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

    ErrorCheck(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
        nullptr, &m_pipeline));

    // Camera Uniform
    {
        if (!m_pCamera)
        {
            Common::Transform camTransform(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
            m_pCamera = std::make_unique<Common::Camera>(camTransform, screenWidth / (float)screenHeight);
        }

        const uint16_t numUniforms = 1;//maxFrameInFlight if camera is moving

        CreateBufferAndMemory(physicalDevice, device, m_cameraUniforms, m_cameraUniformMemory, sizeof(SceneUniform) * numUniforms,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        std::array<SceneUniform, numUniforms> uniformArray{};
        for (auto& uniform : uniformArray)
        {
            uniform.cameraPos = m_pCamera->GetPosition();
            uniform.projectionMat = m_pCamera->GetProjectionMat();
            uniform.viewMat = m_pCamera->GetViewMatrix();
        }

        void* pData;
        ErrorCheck(vkMapMemory(m_device, m_cameraUniformMemory, 0, sizeof(SceneUniform) * numUniforms, 0, &pData));
        memcpy(pData, uniformArray.data(), sizeof(SceneUniform) * numUniforms);
        vkUnmapMemory(m_device, m_cameraUniformMemory);

        {
            m_viewSet.resize(numUniforms);
            VkDescriptorSetAllocateInfo setAllocInfo{};
            setAllocInfo.descriptorPool = m_descriptorPool;
            setAllocInfo.descriptorSetCount = numUniforms;
            setAllocInfo.pSetLayouts = &m_viewSetLayout;
            setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

            ErrorCheck(vkAllocateDescriptorSets(device, &setAllocInfo, m_viewSet.data()));

            for (uint16_t i = 0; i < numUniforms; i++)
            {
                VkDescriptorBufferInfo bufferInfo{ m_cameraUniforms, i*sizeof(SceneUniform), sizeof(SceneUniform)};
                const VkWriteDescriptorSet writes
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_viewSet[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufferInfo, nullptr
                };
                vkUpdateDescriptorSets(device, 1, &writes, 0, nullptr);
            }
        }
    }
}

Common::WireFrameTask::~WireFrameTask()
{
    {
        vkFreeMemory(m_device, m_cameraUniformMemory, nullptr);
        vkDestroyBuffer(m_device, m_cameraUniforms, nullptr);
    }

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyShaderModule(m_device, m_vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);

    //for (uint32_t i = 0; i < m_colorAttachments.size(); i++)
    //{
    //    vkDestroyImageView(m_device, m_colorAttachmentViews[i], nullptr);
    //    vkFreeMemory(m_device, m_colorAttachmentMemory[i], nullptr);
    //    vkDestroyImage(m_device, m_colorAttachments[i], nullptr);
    //}

    //vkDestroyImageView(m_device, m_depthAttachmentViews, nullptr);
    //vkFreeMemory(m_device, m_depthAttachmentMemory, nullptr);
    //vkDestroyImage(m_device, m_depthAttachment, nullptr);

    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_viewSetLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
}

void Common::WireFrameTask::Update(const uint64_t& frameIndex, const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
    uint64_t signalValue, std::optional<uint64_t> waitValue)
{
    /*if (frameIndex > 1)
        BuildCommandBuffers(frameInFlight, true);
    else*/
        BuildCommandBuffers(frameInFlight, false);

    VkSemaphoreSubmitInfo waitInfo{};
    if (waitValue.has_value())
    {
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitInfo.pNext = nullptr;
        waitInfo.semaphore = timelineSem;
        waitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        waitInfo.deviceIndex = 0;
        waitInfo.value = waitValue.value();
    };

    VkSemaphoreSubmitInfo signalInfo
    { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, timelineSem, signalValue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0 };

    VkCommandBufferSubmitInfo bufInfo{};
    bufInfo.commandBuffer = m_commandBuffers[frameInFlight];
    bufInfo.deviceMask = 0;
    bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    VkSubmitInfo2 submitInfo{};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &bufInfo;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    if (waitValue.has_value())
    {
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
    }

    // If the threads are being killed, we need to skip the queue submission to allow the program to exit gracefully
    //if (m_alive)
    {
        ErrorCheck(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    }
}

//const std::vector<VkImage>& Common::WireFrameTask::GetColorAttachments() const
//{
//    return m_colorAttachments;
//}

const std::vector<VkImageView>& Common::WireFrameTask::GetColorAttachmentViews() const
{
    return m_colorAttachmentViews;
}
