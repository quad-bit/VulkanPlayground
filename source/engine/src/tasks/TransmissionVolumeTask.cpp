#include "tasks/TransmissionVolumeTask.h"
#include "LightManager.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "memory/MemoryManager.h"
#include "VulkanWrappers.h"

void Loops::Tasking::TransmissionVolumeTask::Init(
    std::optional<const VkClearColorValue> clearColorValue,
    std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
    const Loops::MaterialManager* pMaterialManager,
    const VkDescriptorSetLayout& transformSetLayout)
{
}

Loops::Tasking::TransmissionVolumeTask::TransmissionVolumeTask(
    const VkUtils::VulkanContext * const vulkanContext,
    uint32_t graphicsQueueFamilyIndex,
    const std::vector<VkDescriptorSet>& sceneSets,
    const std::vector<VkDescriptorSet>& transformSets,
    const VkDescriptorSetLayout& sceneSetLayout,
    const VkDescriptorSetLayout& transformSetLayout,
    const std::vector<VkImageView>& opaqueColorImageCopyViews,
    const std::vector<VkImageView>& opaqueDepthImageCopyViews,
    const std::vector<VkImageView>& backDepthImageViews,
    const std::vector<VkImageView>& colorTargetViews,
    const std::vector<VkImageView>& depthTargetViews,
    const VkFormat& colorFormat, const VkFormat& depthFormat,
    const Loops::MaterialManager* pMaterialManager,
    bool createCommandBuffers) :
    GraphicsTask("TransmissionVolumeTask", vulkanContext, colorTargetViews, depthTargetViews,
        colorFormat, depthFormat),
    m_backDepthImageViews(backDepthImageViews),
    m_opaqueColorImageCopyViews(opaqueColorImageCopyViews),
    m_opaqueDepthImageCopyViews(opaqueDepthImageCopyViews)
{
    if (createCommandBuffers)
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
        // set 0 for scene & Light & directionalShadowMap
        {
            m_customLayout[SCENE_SET] = sceneSetLayout;
        }

        // Transform array set 1
        {
            m_customLayout[TRANSFORM_SET] = transformSetLayout;
        }

        // texture array set 2
        {
            m_customLayout[TEXTURE_SET] = Loops::TextureManager::GetInstance()->GetTextureSetLayout();
        }

        // material array set 3
        {
            VkDescriptorSetLayoutBinding bindings[4]
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
            };

            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 4;
            createInfo.pBindings = &bindings[0];
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_vulkanContext->m_logicalDevice, &createInfo, nullptr, &m_customLayout[MATERIAL_SET]));
        }

        // create only the material descriptor set
        VkDescriptorPoolSize pool_sizes[2] =
        {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * m_vulkanContext->m_maxFrameInFlights},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 * m_vulkanContext->m_maxFrameInFlights}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = 0;
        poolInfo.maxSets = 2 * m_vulkanContext->m_maxFrameInFlights;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = pool_sizes;
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(m_vulkanContext->m_logicalDevice, &poolInfo, nullptr, &m_descriptorPool));
    }

    {
        std::array<VkPushConstantRange, 1> range
        {
            VkPushConstantRange
            {
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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
    }

    // pipeline
    {
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_colorFormat;
        pipelineRenderingCreateInfo.depthAttachmentFormat = m_depthFormat;
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        // Create pipeline
        std::string vertSpvPath = std::string{ SPV_PATH } + "VolumeTransmissionVert.spv";
        std::string fragSpvPath = std::string{ SPV_PATH } + "VolumeTransmissionFrag.spv";

        VkPipelineShaderStageCreateInfo vertShaderStage, fragShaderStage;
        std::tie(m_vertexShaderModule, vertShaderStage) = Loops::VkUtils::CreateShaderModule(m_vulkanContext->m_logicalDevice, vertSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);
        std::tie(m_fragmentShaderModule, fragShaderStage) = Loops::VkUtils::CreateShaderModule(m_vulkanContext->m_logicalDevice, fragSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
        pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Describe the vertex input, i.e. two vertex input attributes in our case:
        VkVertexInputBindingDescription vertexBindings{ 0, sizeof(Loops::Vertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX };
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions
        {
            VkVertexInputAttributeDescription{0, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Loops::Vertex, Loops::Vertex::m_position)},
            VkVertexInputAttributeDescription{1, 0, VkFormat::VK_FORMAT_R32G32B32_SFLOAT, offsetof(Loops::Vertex, Loops::Vertex::m_normal)},
            VkVertexInputAttributeDescription{2, 0, VkFormat::VK_FORMAT_R32G32_SFLOAT, offsetof(Loops::Vertex, Loops::Vertex::m_uv)},
            VkVertexInputAttributeDescription{3, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Loops::Vertex, Loops::Vertex::m_tangent)}
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
        pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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

        std::array<VkSpecializationMapEntry, 1> specializationMapEntries;
        specializationMapEntries[0].constantID = 0;
        specializationMapEntries[0].size = sizeof(LightManager::MAX_LIGHTS);
        specializationMapEntries[0].offset = 0;

        VkSpecializationInfo specializationInfo{};
        specializationInfo.dataSize = sizeof(LightManager::MAX_LIGHTS);
        specializationInfo.mapEntryCount = specializationMapEntries.size();
        specializationInfo.pData = &LightManager::MAX_LIGHTS;
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
        pipelineShaderStageCreateInfos[1].pSpecializationInfo = &specializationInfo;

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
        const bool clearTargetOnLoad{ false };
        m_renderInfoList = VkUtils::CreateRenderingInfo(
            colorTargetViews, depthTargetViews,
            VkClearColorValue{ 1.0f, 1.0f, 1.0f, 1.0f },// dummy as not clearing
            VkClearDepthStencilValue{ 1.0f, 0 },// dummy as not clearing
            m_vulkanContext->m_renderDimensions.m_width,
            m_vulkanContext->m_renderDimensions.m_height,
            m_colorInfoList, m_depthInfoList,
            clearTargetOnLoad
        );
    }

    // Scene set 0 is handled in Opaque
    // Transform set 1 is handled in SceneManager
    // Texture set 2 is handled in TextureManager
    // Material set 3
    {
        const uint16_t numUniforms = m_vulkanContext->m_maxFrameInFlights;// as set contains the opaque renderTargets also

        m_materialUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_vulkanContext->m_physicalDevice, sizeof(VolumeMaterialUniform) * MaterialManager::MAX_MATERIALS);
        VkUtils::CreateBufferVma(m_materialUniformDataSizePerFrame * numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vkBuffer, m_materialBuffer.m_vmaAllocation);
        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vmaAllocation, &m_materialUniformMemoryPointer);
        ASSERT_MSG(m_materialUniformMemoryPointer != nullptr, "not mapped");

        {
            const auto& materialMap = pMaterialManager->GetSceneMaterials();
            m_materialArray.resize(MaterialManager::MAX_MATERIALS);
            for (const auto& [index, material] : materialMap)
            {
                if (material.m_effect == EFFECT_TYPE::TRANSLUCENT_EFT && (material.m_techniqueType == TECHNIQUE_TYPE::VOLUME_TRANSMISSION))
                {
                    /*m_materialArray[index].m_color = material.m_materialData->m_baseColorFactor;
                    m_materialArray[index].m_diffuseMapIndex = material.m_materialData->m_baseColorTextureIndex;
                    const PbrMaterial* pbr = static_cast<const PbrMaterial*>(material.m_materialData);
                    m_materialArray[index].m_normalMapIndex = pbr->m_normalTextureIndex;*/

                    const VolumeTransmissionMaterial* volTr = static_cast<const VolumeTransmissionMaterial*>(material.m_materialData);

                    m_materialArray[index].m_attenuationColor = volTr->m_attenuationColor;
                    m_materialArray[index].m_attenuationDistance = volTr->m_thicknessFactor;
                    m_materialArray[index].m_metallicRoughnessTextureIndex = volTr->m_metallicRoughnessTextureIndex;
                    m_materialArray[index].m_normalTextureIndex = volTr->m_normalTextureIndex;
                    m_materialArray[index].m_transmissionFactor = volTr->m_transmissionFactor;
                }
                /*else
                    ASSERT_MSG_DEBUG(0, "case not handled");*/
            }

            ASSERT_MSG(m_materialUniformMemoryPointer != nullptr, "not yet mapped");
            memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_materialUniformMemoryPointer)), m_materialArray.data(), sizeof(VolumeMaterialUniform) * MaterialManager::MAX_MATERIALS);
        }

        // Sampler
        {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.compareEnable = VK_TRUE;                 // Enable comparison
            samplerInfo.compareOp = VK_COMPARE_OP_LESS;          // Compare reference vs sampled depth
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 1.0f;

            VkUtils::ErrorCheck(vkCreateSampler(m_vulkanContext->m_logicalDevice, &samplerInfo, nullptr, &m_sampler));
        }

        {
            m_materialSet.resize(numUniforms);

            for (uint16_t i = 0; i < numUniforms; i++)
            {
                VkDescriptorSetAllocateInfo setAllocInfo{};
                setAllocInfo.descriptorPool = m_descriptorPool;
                setAllocInfo.descriptorSetCount = 1;
                setAllocInfo.pSetLayouts = &m_customLayout[MATERIAL_SET];
                setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_vulkanContext->m_logicalDevice, &setAllocInfo, &m_materialSet[i]));

                const VkDescriptorBufferInfo bufferInfo{ m_materialBuffer.m_vkBuffer, i * m_materialUniformDataSizePerFrame, m_materialUniformDataSizePerFrame };
                const VkDescriptorImageInfo sceneColorImageInfo{ m_sampler, m_opaqueColorImageCopyViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                const VkDescriptorImageInfo sceneDepthImageInfo{ m_sampler, m_opaqueDepthImageCopyViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                const VkDescriptorImageInfo backDepthImageInfo{ m_sampler, m_backDepthImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                const VkWriteDescriptorSet writes[4]
                {
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_materialSet[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr},
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_materialSet[i], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneColorImageInfo, nullptr, nullptr},
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_materialSet[i], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneDepthImageInfo, nullptr, nullptr },
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_materialSet[i], 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &backDepthImageInfo, nullptr, nullptr }
                };
                vkUpdateDescriptorSets(m_vulkanContext->m_logicalDevice, 4, writes, 0, nullptr);
            }
        }
    }
}

void Loops::Tasking::TransmissionVolumeTask::Update(const uint32_t& frameInFlight,
    const VkSemaphore& timelineSem, uint64_t signalValue,
    std::optional<uint64_t> waitValue, const Loops::RenderData& renderData,
    const Loops::SceneManager& sceneManager, 
    const std::unordered_map<uint32_t, Loops::Material>& materials,
    const VkDescriptorSet& transformSet, const VkDescriptorSet& sceneSet)
{
    // Build Command Buffers
    {
        VkViewport viewport = { 0.0f, static_cast<float>(m_vulkanContext->m_renderDimensions.m_height), static_cast<float>(m_vulkanContext->m_renderDimensions.m_width), -static_cast<float>(m_vulkanContext->m_renderDimensions.m_height), 0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, {m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height} };

        Loops::VkUtils::ErrorCheck(vkResetCommandBuffer(m_commandBuffers[frameInFlight], 0));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &beginInfo));

        auto RenderCommands = [this, &viewport, &scissor, &sceneSet, &renderData, &sceneManager, &materials, &transformSet](uint32_t frameInFlight)
            {
                vkCmdBeginRendering(m_commandBuffers[frameInFlight], &m_renderInfoList[frameInFlight]);
                {
                    vkCmdSetViewport(m_commandBuffers[frameInFlight], 0, 1, &viewport);
                    vkCmdSetScissor(m_commandBuffers[frameInFlight], 0, 1, &scissor);

                    int boundVertexBuffer = -1, boundIndexBuffer = -1;
                    auto techIt = renderData.m_drawablesPerMaterial.find(Loops::EFFECT_TYPE::TRANSLUCENT_EFT);
                    if (techIt != renderData.m_drawablesPerMaterial.end())
                    {
                        // Bind descriptor sets
                        // Scene set 0
                        // Transform set 1
                        // Texture set 2
                        // Materials set 3
                        VkDescriptorSet set[4]{ sceneSet,
                            transformSet,
                            TextureManager::GetInstance()->GetTextureSet()[frameInFlight],
                            m_materialSet[0]
                        };

                        VkBindDescriptorSetsInfo bindInfo = {};
                        bindInfo.descriptorSetCount = 4;
                        bindInfo.firstSet = 0;
                        bindInfo.layout = m_pipelineLayout;
                        bindInfo.pDescriptorSets = set;
                        bindInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO;
                        bindInfo.dynamicOffsetCount = 0;
                        bindInfo.pDynamicOffsets = nullptr;
                        vkCmdBindDescriptorSets2(m_commandBuffers[frameInFlight], &bindInfo);

                        auto RecordDraw = [&renderData, &boundIndexBuffer,
                            &boundVertexBuffer, frameInFlight,
                            &sceneManager, this,
                            materials](const std::vector<uint32_t>& drawableIndicies)
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
                                        vkCmdBindVertexBuffers(m_commandBuffers[frameInFlight], 0, 1, &vertexBuffer, &offset);
                                        vkCmdBindIndexBuffer(m_commandBuffers[frameInFlight], indexBuffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
                                    }

                                    // Push Constant 
                                    uint32_t matrixIndex = drawable.m_matrixIndex;

                                    // Launch draw
                                    for (uint32_t i = 0; i < drawable.m_numOfViews; i++)
                                    {
                                        const Loops::MeshView& meshView = renderData.m_meshViews[drawable.m_viewStartIndex + i];
                                        auto materialIndex = meshView.m_materialIndex;
                                        auto& material = materials.at(materialIndex);
                                        uint32_t textureIndex = ((PbrMaterial*)material.m_materialData)->m_baseColorTextureIndex;
                                        PushConsts pushConsts{ (int)matrixIndex, (int)materialIndex };
                                        VkPushConstantsInfo info{};
                                        info.layout = m_pipelineLayout;
                                        info.offset = 0;
                                        info.pValues = &pushConsts;
                                        info.size = sizeof(PushConsts);
                                        info.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                                        info.sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO;
                                        vkCmdPushConstants2(m_commandBuffers[frameInFlight], &info);

                                        uint32_t numIndicies = meshView.m_indexCount;
                                        uint32_t firstIndex = meshView.m_firstIndex;
                                        vkCmdDrawIndexed(m_commandBuffers[frameInFlight], numIndicies, 1, firstIndex, 0, 0);
                                    }
                                }
                            };

                        auto volTrIt = techIt->second.find(Loops::TECHNIQUE_TYPE::VOLUME_TRANSMISSION);
                        if (volTrIt != techIt->second.end())
                        {
                            vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                            const std::vector<uint32_t>& drawableIndicies = volTrIt->second;
                            RecordDraw(drawableIndicies);
                        }
                    }
                }
                vkCmdEndRendering(m_commandBuffers[frameInFlight]);
            };

        RenderCommands(frameInFlight);

        Loops::VkUtils::ErrorCheck(vkEndCommandBuffer(m_commandBuffers[frameInFlight]));
    }

    Submit(frameInFlight, timelineSem, signalValue, waitValue);
}

Loops::Tasking::TransmissionVolumeTask::~TransmissionVolumeTask()
{
    vkDestroyDescriptorSetLayout(m_vulkanContext->m_logicalDevice, m_customLayout[MATERIAL_SET], nullptr);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vkBuffer, m_materialBuffer.m_vmaAllocation);

    vkDestroySampler(m_vulkanContext->m_logicalDevice, m_sampler, nullptr);
}
