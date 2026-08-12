#include "tasks/PhongShadingTask.h"
#include "LightManager.h"
#include "TextureManager.h"
#include "MaterialManager.h"

void Loops::Tasking::PhongShadingTask::Init(std::optional<const VkClearColorValue> clearColorValue,
    std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
    const Loops::MaterialManager* pMaterialManager)
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
        // set 0 for scene & Light & directionalShadowMap & directionalShadowMap
        {
            VkDescriptorSetLayoutBinding bindings[3]
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                LightManager::GetInstance()->GetLightDataBinding(),
                LightManager::GetInstance()->GetDirectionalShadowMapBinding()
            };

            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 3;
            createInfo.pBindings = &bindings[0];
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_info.m_device, &createInfo, nullptr, &m_customLayout[SCENE_SET]));
        }

        // Transform array set 1
        {
            VkDescriptorSetLayoutBinding bindings[1]
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
            };

            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 1;
            createInfo.pBindings = &bindings[0];
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_info.m_device, &createInfo, nullptr, &m_customLayout[TRANSFORM_SET]));
        }

        // texture array set 2
        {
            m_customLayout[TEXTURE_SET] = Loops::TextureManager::GetInstance()->GetTextureSetLayout();
        }

        // material array set 3
        {
            VkDescriptorSetLayoutBinding bindings[1]
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
            };

            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 1;
            createInfo.pBindings = &bindings[0];
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_info.m_device, &createInfo, nullptr, &m_customLayout[MATERIAL_SET]));
        }

        VkDescriptorPoolSize pool_sizes[3] =
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 * m_info.m_maxFrameInFlights},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * m_info.m_maxFrameInFlights},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 * m_info.m_maxFrameInFlights}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = 0;
        poolInfo.maxSets = 4 * m_info.m_maxFrameInFlights;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = pool_sizes;
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(m_info.m_device, &poolInfo, nullptr, &m_descriptorPool));
    }

    /*layout(push_constant) uniform PushConsts
    {
        int transformIndex;
        int materialIndex;
    };*/


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

    Loops::VkUtils::ErrorCheck(vkCreatePipelineLayout(m_info.m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

    // pipeline
    {
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_colorFormat;
        pipelineRenderingCreateInfo.depthAttachmentFormat = m_depthFormat;
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        // Create pipeline
        std::string vertSpvPath = std::string{ SPV_PATH } + "PhongShadingVert.spv";
        std::string fragSpvPath = std::string{ SPV_PATH } + "PhongShadingFrag.spv";

        VkPipelineShaderStageCreateInfo vertShaderStage, fragShaderStage;
        std::tie(m_vertexShaderModule, vertShaderStage) = Loops::VkUtils::CreateShaderModule(m_info.m_device, vertSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);
        std::tie(m_fragmentShaderModule, fragShaderStage) = Loops::VkUtils::CreateShaderModule(m_info.m_device, fragSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);

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

        Loops::VkUtils::ErrorCheck(vkCreateGraphicsPipelines(m_info.m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
            nullptr, &m_pipeline));

        pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
        Loops::VkUtils::ErrorCheck(vkCreateGraphicsPipelines(m_info.m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
            nullptr, &m_doubleSidedPipeline));
    }


    if (!m_ownAttachments)
    {
        //Render pass
        VkClearValue clearValues{ clearColorValue.has_value() ? clearColorValue.value() : VkClearColorValue{.0f, .0f, .0f, 1.0f} };
        m_colorInfoList.resize(m_info.m_maxFrameInFlights);

        VkClearValue clearValuesDepth;
        clearValuesDepth.depthStencil = depthStencilClearValue.has_value() ? depthStencilClearValue.value() : VkClearDepthStencilValue{ 1.0f, 0u };
        m_depthInfoList.resize(m_info.m_maxFrameInFlights);

        for (uint32_t i = 0; i < m_colorAttachmentViews.size(); i++)
        {
            m_colorInfoList[i].clearValue = clearValues;
            m_colorInfoList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            m_colorInfoList[i].imageView = m_colorAttachmentViews[i];
            m_colorInfoList[i].loadOp = clearColorValue.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            m_colorInfoList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            m_colorInfoList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        }

        for (uint32_t i = 0; i < m_depthAttachmentViews.size(); i++)
        {
            m_depthInfoList[i].clearValue = clearValuesDepth;
            m_depthInfoList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            m_depthInfoList[i].imageView = m_depthAttachmentViews[i];
            m_depthInfoList[i].loadOp = depthStencilClearValue.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            m_depthInfoList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            m_depthInfoList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        }

        for (uint32_t i = 0; i < m_info.m_maxFrameInFlights; i++)
        {
            VkRenderingInfo info{};
            info.colorAttachmentCount = (1);
            info.layerCount = (1);
            info.pColorAttachments = &m_colorInfoList[i];
            info.pDepthAttachment = (m_depthAttachmentViews.size() == m_colorAttachmentViews.size()) ? &m_depthInfoList[i] : &m_depthInfoList[0];
            info.renderArea = VkRect2D{ {0, 0}, {m_info.m_renderDimensions.m_width, m_info.m_renderDimensions.m_height} };
            info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            m_renderInfoList.push_back(std::move(info));
        }
    }

    // scene set 0
    // binding 0 camera
    // binding 1 lightdata
    // binding 2 directionalShadowMap
    {
        const uint16_t numUniforms = m_info.m_maxFrameInFlights;

        // camera
        m_cameraUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_info.m_physicalDevice, sizeof(CameraData));
        VkUtils::CreateBufferVma(m_cameraUniformDataSizePerFrame * numUniforms, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vkBuffer, m_cameraBuffer.m_vmaAllocation);
        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vmaAllocation, &m_cameraUniformMemoryPointer);
        ASSERT_MSG(m_cameraUniformMemoryPointer != nullptr, "not mapped");

        // light
        m_lightUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_info.m_physicalDevice, sizeof(LightUniform) * LightManager::MAX_LIGHTS);
        VkUtils::CreateBufferVma(m_lightUniformDataSizePerFrame * numUniforms, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_lightDataBuffer.m_vkBuffer, m_lightDataBuffer.m_vmaAllocation);
        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_lightDataBuffer.m_vmaAllocation, &m_lightUniformMemoryPointer);
        ASSERT_MSG(m_lightUniformMemoryPointer != nullptr, "not mapped");

        m_sceneSet.resize(numUniforms);
        for (uint16_t i = 0; i < numUniforms; i++)
        {
            VkDescriptorSetAllocateInfo setAllocInfo{};
            setAllocInfo.descriptorPool = m_descriptorPool;
            setAllocInfo.descriptorSetCount = 1;
            setAllocInfo.pSetLayouts = &m_customLayout[SCENE_SET];
            setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

            Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_info.m_device, &setAllocInfo, &m_sceneSet[i]));

            {
                VkDescriptorBufferInfo cameraBufferInfo{ m_cameraBuffer.m_vkBuffer, i * m_cameraUniformDataSizePerFrame, sizeof(CameraData) };
                VkDescriptorBufferInfo lightBufferInfo{ m_lightDataBuffer.m_vkBuffer, i * m_lightUniformDataSizePerFrame, sizeof(LightUniform) * LightManager::MAX_LIGHTS };

                // Get the shadowMaps from LightManager
                const uint32_t whiteTextureIndex = 0;
                auto [image, imageView] = TextureManager::GetInstance()->GetImage(whiteTextureIndex);
                auto sampler = TextureManager::GetInstance()->GetSampler(whiteTextureIndex);
                VkDescriptorImageInfo directionalShadowMapInfo{sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

                const VkWriteDescriptorSet writes[3]
                {
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_sceneSet[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &cameraBufferInfo, nullptr},
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_sceneSet[i], 1, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &lightBufferInfo, nullptr},
                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_sceneSet[i], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &directionalShadowMapInfo, nullptr, nullptr}
                };
                vkUpdateDescriptorSets(m_info.m_device, 3, writes, 0, nullptr);
            }
        }
    }

    // Transform set 1
    {
        const uint16_t numUniforms = m_info.m_maxFrameInFlights;

        const size_t dataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_info.m_physicalDevice, sizeof(glm::mat4) * MAX_ENTITIES);
        m_transformUniformDataSizePerFrame = dataSizePerFrame;

        VkUtils::CreateBufferVma(dataSizePerFrame * numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vmaAllocation, &m_transformUniformMemoryPointer);
        ASSERT_MSG(m_transformUniformMemoryPointer != nullptr, "not mapped");

        {
            m_transformSets.resize(numUniforms);

            for (uint16_t i = 0; i < numUniforms; i++)
            {
                VkDescriptorSetAllocateInfo setAllocInfo{};
                setAllocInfo.descriptorPool = m_descriptorPool;
                setAllocInfo.descriptorSetCount = 1;
                setAllocInfo.pSetLayouts = &m_customLayout[TRANSFORM_SET];
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
    // Texture set 2 is handled in TextureManager
    // Material set 3
    {
        const uint16_t numUniforms = 1;// m_info.m_maxFrameInFlights;

        m_materialUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_info.m_physicalDevice, sizeof(PhongMaterialUniform) * MaterialManager::MAX_MATERIALS);
        VkUtils::CreateBufferVma(m_materialUniformDataSizePerFrame * numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vkBuffer, m_materialBuffer.m_vmaAllocation);
        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vmaAllocation, &m_materialUniformMemoryPointer);
        ASSERT_MSG(m_materialUniformMemoryPointer != nullptr, "not mapped");

        {
            const auto& materialMap = pMaterialManager->GetSceneMaterials();
            m_materialArray.resize(MaterialManager::MAX_MATERIALS);
            for (const auto& [index, material] : materialMap)
            {
                if (material.m_effect == EFFECT_TYPE::OPAQUE_EFT && (material.m_techniqueType == TECHNIQUE_TYPE::PBR || material.m_techniqueType == TECHNIQUE_TYPE::PBR_DOUBLE_SIDED))
                {
                    m_materialArray[index].m_color = material.m_materialData->m_baseColorFactor;
                    m_materialArray[index].m_diffuseMapIndex = material.m_materialData->m_baseColorTextureIndex;
                    const PbrMaterial* pbr = static_cast<const PbrMaterial*>(material.m_materialData);
                    m_materialArray[index].m_normalMapIndex = pbr->m_normalTextureIndex;
                }
                /*else
                    ASSERT_MSG_DEBUG(0, "case not handled");*/
            }

            ASSERT_MSG(m_materialUniformMemoryPointer != nullptr, "not yet mapped");
            memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_materialUniformMemoryPointer)), m_materialArray.data(), sizeof(PhongMaterialUniform) * MaterialManager::MAX_MATERIALS);
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
                Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_info.m_device, &setAllocInfo, &m_materialSet[i]));

                VkDescriptorBufferInfo bufferInfo{ m_materialBuffer.m_vkBuffer, i * m_materialUniformDataSizePerFrame, m_materialUniformDataSizePerFrame };
                const VkWriteDescriptorSet writes
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_materialSet[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr
                };
                vkUpdateDescriptorSets(m_info.m_device, 1, &writes, 0, nullptr);
            }
        }
    }
}

Loops::Tasking::PhongShadingTask::PhongShadingTask(const GraphicsTaskInfo& info,
    const std::vector<VkImageView>& colorViews,
    const std::vector<VkImageView>& depthViews,
    const VkFormat& colorFormat, const VkFormat& depthFormat,
    std::optional<const VkClearColorValue> clearColorValue,
    std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
    const Loops::MaterialManager* pMaterialManager) :
    GraphicsTask("PhongShadingTask", info, colorViews, depthViews,
        colorFormat, depthFormat)
{
    Init(clearColorValue, depthStencilClearValue, pMaterialManager);
}

void Loops::Tasking::PhongShadingTask::Update(const uint32_t& frameInFlight,
    const VkSemaphore& timelineSem, uint64_t signalValue,
    std::optional<uint64_t> waitValue, const Loops::RenderData& renderData,
    const Loops::SceneManager& sceneManager, const std::unordered_map<uint32_t,
    Loops::Material>& materials)
{
    {
        // set 0 binding 0 camera
        {
            CameraData uniform{ renderData.m_cameraData.m_viewMat, renderData.m_cameraData.m_projectionMat, renderData.m_cameraData.m_cameraPos };
            ASSERT_MSG(m_cameraUniformMemoryPointer != nullptr, "not yet mapped");
            memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_cameraUniformMemoryPointer) + m_cameraUniformDataSizePerFrame * frameInFlight), &uniform, sizeof(CameraData));
        }

        // set 0 binding 1 Light
        {
            const auto& lightUniforms = LightManager::GetInstance()->GetLightUniformArray();

            ASSERT_MSG(m_lightUniformMemoryPointer != nullptr, "not yet mapped");
            memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_lightUniformMemoryPointer) + m_lightUniformDataSizePerFrame * frameInFlight), lightUniforms.data(), sizeof(LightUniform) * lightUniforms.size());
        }
    }

    // set 1 binding 0 transform array
    {
        ASSERT_MSG(m_transformUniformMemoryPointer != nullptr, "not yet mapped");
        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_transformUniformMemoryPointer) + m_transformUniformDataSizePerFrame * frameInFlight), renderData.m_modelMats, sizeof(glm::mat4) * renderData.m_drawableCount);
    }

    // set 2 binding 0 texture array
    {
        // handled in TextureManager
    }

    // set 3 binding 0 material array
    {
        // copied in Init as its not changing during the update
    }

    // Build Command Buffers
    {
        VkViewport viewport = { 0.0f, static_cast<float>(m_info.m_renderDimensions.m_height), static_cast<float>(m_info.m_renderDimensions.m_width), -static_cast<float>(m_info.m_renderDimensions.m_height), 0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, {m_info.m_renderDimensions.m_width, m_info.m_renderDimensions.m_height} };

        Loops::VkUtils::ErrorCheck(vkResetCommandBuffer(m_commandBuffers[frameInFlight], 0));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &beginInfo));

        auto RenderCommands = [this, &viewport, &scissor, &renderData, &sceneManager, &materials](uint32_t frameInFlight)
            {
                vkCmdBeginRendering(m_commandBuffers[frameInFlight], &m_renderInfoList[frameInFlight]);
                {
                    vkCmdSetViewport(m_commandBuffers[frameInFlight], 0, 1, &viewport);
                    vkCmdSetScissor(m_commandBuffers[frameInFlight], 0, 1, &scissor);
                    //vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

                    // Bind descriptor sets
                    // Scene set 0
                    // Transform set 1
                    // Texture set 2
                    // Materials set 3
                    /*VkDescriptorSet set[4]{ m_sceneSet[frameInFlight],
                        m_transformSets[frameInFlight],
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
                    vkCmdBindDescriptorSets2(m_commandBuffers[frameInFlight], &bindInfo);*/

                    int boundVertexBuffer = -1, boundIndexBuffer = -1;
                    auto techIt = renderData.m_drawablesPerMaterial.find(Loops::EFFECT_TYPE::OPAQUE_EFT);
                    if (techIt != renderData.m_drawablesPerMaterial.end())
                    {
                        //vkCmdSetViewport(m_commandBuffers[frameInFlight], 0, 1, &viewport);
                        //vkCmdSetScissor(m_commandBuffers[frameInFlight], 0, 1, &scissor);

                        // Bind descriptor sets
                        // Scene set 0
                        // Transform set 1
                        // Texture set 2
                        // Materials set 3
                        VkDescriptorSet set[4]{ m_sceneSet[frameInFlight],
                            m_transformSets[frameInFlight],
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

                        auto pbrIt = techIt->second.find(Loops::TECHNIQUE_TYPE::PBR);
                        auto pbrDSidedIt = techIt->second.find(Loops::TECHNIQUE_TYPE::PBR_DOUBLE_SIDED);
                        if (pbrIt != techIt->second.end())
                        {
                            vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                            const std::vector<uint32_t>& drawableIndicies = pbrIt->second;
                            RecordDraw(drawableIndicies);
                        }
                        
                        if (pbrDSidedIt != techIt->second.end())
                        {
                            vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_doubleSidedPipeline);
                            const std::vector<uint32_t>& drawableIndicies = pbrDSidedIt->second;
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

void Loops::Tasking::PhongShadingTask::Update(VkCommandBuffer& commandBuffer,
    const uint32_t& frameInFlight, const Loops::RenderData& renderData,
    const Loops::SceneManager& sceneManager, 
    std::optional<CameraData> secondaryCameraData)
{
}

Loops::Tasking::PhongShadingTask::~PhongShadingTask()
{
    vkDestroyDescriptorSetLayout(m_info.m_device, m_customLayout[0], nullptr);
    vkDestroyDescriptorSetLayout(m_info.m_device, m_customLayout[1], nullptr);
    vkDestroyDescriptorSetLayout(m_info.m_device, m_customLayout[3], nullptr);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vkBuffer, m_cameraBuffer.m_vmaAllocation);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vkBuffer, m_materialBuffer.m_vmaAllocation);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_lightDataBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_lightDataBuffer.m_vkBuffer, m_lightDataBuffer.m_vmaAllocation);

    vkDestroyPipeline(m_info.m_device, m_doubleSidedPipeline, nullptr);
}
