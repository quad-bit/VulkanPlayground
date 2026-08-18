#include "LightManager.h"
#include "Assertion.h"
#include "TextureManager.h"
#include "memory/MemoryManager.h"

#include <array>

// Initialize static members
Loops::LightManager* Loops::LightManager::s_instancePtr = nullptr;
std::mutex Loops::LightManager::s_mtx;

Loops::LightManager* Loops::LightManager::GetInstance()
{
    if (s_instancePtr == nullptr)
    {
        std::lock_guard<std::mutex> lock(s_mtx);
        if (s_instancePtr == nullptr)
        {
            s_instancePtr = new Loops::LightManager();
        }
    }
    return s_instancePtr;
}


Loops::LightManager::LightManager()
{
    // meant for opaque and transparent effects

    m_lightDataBinding = 
    {
        1, // binding slot
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1, // num bindings
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr // mutable sampler
    };

    m_directionalShadowMapBinding =
    {
        2,//binding location
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };

    m_pointShadowMapBinding = 
    {
        3,//binding location
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        s_maxCubeShadowMaps,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };
}

void Loops::LightManager::DeInitPrivate()
{
    VkUtils::DestroyRenderTargets(nullptr, 0,
        m_depthTargets.data(), m_depthTargets.size(),
        m_vulkanContext.m_logicalDevice);

    vkDestroyDescriptorSetLayout(m_vulkanContext.m_logicalDevice, m_layout[SCENE_SET], nullptr);
    vkDestroyDescriptorSetLayout(m_vulkanContext.m_logicalDevice, m_layout[TRANSFORM_SET], nullptr);

    vkDestroyCommandPool(m_vulkanContext.m_logicalDevice, m_commandPool, nullptr);
    vkDestroyDescriptorPool(m_vulkanContext.m_logicalDevice, m_descriptorPool, nullptr);

    vkDestroyShaderModule(m_vulkanContext.m_logicalDevice, m_vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_vulkanContext.m_logicalDevice, m_fragmentShaderModule, nullptr);

    vkDestroyPipelineLayout(m_vulkanContext.m_logicalDevice, m_pipelineLayout, nullptr);
    vkDestroyPipeline(m_vulkanContext.m_logicalDevice, m_pipeline, nullptr);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vkBuffer, m_cameraBuffer.m_vmaAllocation);

    vkDestroySampler(m_vulkanContext.m_logicalDevice, m_shadowSampler, nullptr);
}

void Loops::LightManager::DeInit()
{
    s_instancePtr->DeInitPrivate();
    delete s_instancePtr;
}

void Loops::LightManager::Init(flecs::world& world, const VkUtils::VulkanContext& vulkanContext)
{
    world.system<Transform, Light>("LightSystem")
        //.kind(0)
        .each(
            [this](Transform& t, Light& l)
            {
                const LIGHT_TYPE lightType = l.m_type;
                if (lightType == LIGHT_TYPE::DIRECTIONAL)
                {
                    auto translationMat = glm::translate(t.m_position);
                    auto scaleMat = glm::scale(t.m_scale);

                    glm::mat4 rotXMat = glm::rotate(t.m_eulerAngles.x, glm::vec3(1, 0, 0));
                    glm::mat4 rotYMat = glm::rotate(t.m_eulerAngles.y, glm::vec3(0, 1, 0));
                    glm::mat4 rotZMat = glm::rotate(t.m_eulerAngles.z, glm::vec3(0, 0, 1));

                    auto rotationMat = rotZMat * rotYMat * rotXMat;

                    t.m_modelMat = translationMat * rotationMat * scaleMat;

                    glm::vec3 forward{ t.m_modelMat[2] };
                    forward = glm::normalize(forward);

                    DirectionalLight* dirLight = static_cast<DirectionalLight*>(l.m_data);
                    dirLight->m_direction = forward;

                    // directional light is always at 0
                    m_lightUniformArray[0].m_ambient = glm::vec4(dirLight->m_ambient.x, dirLight->m_ambient.y, dirLight->m_ambient.z, 1.0);
                    m_lightUniformArray[0].m_diffuse = glm::vec4(dirLight->m_diffuse.x, dirLight->m_diffuse.y, dirLight->m_diffuse.z, 1.0);
                    m_lightUniformArray[0].m_specular = glm::vec4(dirLight->m_specular.x, dirLight->m_specular.y, dirLight->m_specular.z, 1.0);;
                    //m_lightUniformArray[0].m_lightMatrix = t.m_modelMat;
                    m_lightUniformArray[0].m_posOrDir = glm::vec4{ forward.x, forward.y, forward.z, 0.0 };
                    m_lightUniformArray[0].m_shadowMapIndex = -1;

                    float nearPlane = 1.0f, farPlane = 50.5f;
                    glm::mat4 orthoProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, nearPlane, farPlane);
                    glm::mat4 view = glm::lookAt(t.m_position, glm::normalize(t.m_position + glm::vec3(t.m_modelMat[2])), glm::vec3(0.0f, 1.0f, 0.0f));

                    m_lightUniformArray[0].m_lightMatrix = orthoProjection * view;
                    m_lightTransforms.push_back(t.m_modelMat);
                }
                else
                    ASSERT_MSG_DEBUG(0, "not yet handled");
            }
        );

    /*world.system<Transform, Mesh>("ShadowSystem")
        .each(
            [this](const Transform& t, const Mesh& m)
            {
                
            }
        );*/

    // vulkan pass setup
    {
        m_vulkanContext = vulkanContext;

        // setup render target for depth pass
        {
            m_depthTargets.resize(vulkanContext.m_maxFrameInFlights);
            m_renderAttachmentInfoList.resize(vulkanContext.m_maxFrameInFlights);
            std::vector<VkRenderingAttachmentInfo> temp;

            VkFormat tempFormat;
            VkClearDepthStencilValue depthClearValue = VkClearDepthStencilValue{ 1.0f, 0 };
            VkFormat depthFormatValue = TextureManager::GetInstance()->GetBestFormat(TEXTURE_TYPE::DEPTH_STENCIL, false);

            m_renderingInfo = VkUtils::CreateRendertargets(nullptr, 0,
                m_depthTargets.data(), m_depthTargets.size(),
                tempFormat, depthFormatValue,
                SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT,
                vulkanContext.m_logicalDevice,
                {}, depthClearValue,
                temp, m_renderAttachmentInfoList);

            std::vector<VkImage> images;
            for (auto& target : m_depthTargets)
            {
                images.push_back(target.m_vkImage);
                m_depthTargetLayouts.push_back(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }

            VkUtils::ChangeImageLayout(vulkanContext.m_logicalDevice,
                images, vulkanContext.m_graphicsQueue,
                vulkanContext.m_graphicsQueueFamilyIndex,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        //setup Command and descriptor pools, descriptor layout
        {
            VkCommandPoolCreateInfo createInfo{};
            createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            createInfo.queueFamilyIndex = m_vulkanContext.m_graphicsQueueFamilyIndex;
            createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

            Loops::VkUtils::ErrorCheck(vkCreateCommandPool(m_vulkanContext.m_logicalDevice, &createInfo, nullptr, &m_commandPool));

            m_commandBuffers.resize(m_vulkanContext.m_maxFrameInFlights);
            VkCommandBufferAllocateInfo alloc_info{};
            alloc_info.commandBufferCount = m_vulkanContext.m_maxFrameInFlights;
            alloc_info.commandPool = m_commandPool;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

            Loops::VkUtils::ErrorCheck(vkAllocateCommandBuffers(m_vulkanContext.m_logicalDevice, &alloc_info, &m_commandBuffers[0]));

            {
                constexpr uint16_t numSets{ 2 };
                m_layout.resize(numSets);
                // set 0 for scene & Light & directionalShadowMap & directionalShadowMap
                {
                    VkDescriptorSetLayoutBinding bindings[1]
                    {
                        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
                    };

                    VkDescriptorSetLayoutCreateInfo createInfo{};
                    createInfo.bindingCount = 1;
                    createInfo.pBindings = &bindings[0];
                    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_vulkanContext.m_logicalDevice, &createInfo, nullptr, &m_layout[SCENE_SET]));
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
                    Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_vulkanContext.m_logicalDevice, &createInfo, nullptr, &m_layout[TRANSFORM_SET]));
                }

                VkDescriptorPoolSize pool_sizes[2] =
                {
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_vulkanContext.m_maxFrameInFlights},
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_vulkanContext.m_maxFrameInFlights}
                };

                VkDescriptorPoolCreateInfo poolInfo{};
                poolInfo.flags = 0;
                poolInfo.maxSets = 2 * m_vulkanContext.m_maxFrameInFlights;
                poolInfo.poolSizeCount = 2;
                poolInfo.pPoolSizes = pool_sizes;
                poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(m_vulkanContext.m_logicalDevice, &poolInfo, nullptr, &m_descriptorPool));
            }
        }

        // push_constants and pipeline layout
        {
            std::array<VkPushConstantRange, 1> range
            {
                VkPushConstantRange
                {
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(uint32_t)
                }
            };

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
            pipelineLayoutCreateInfo.pPushConstantRanges = range.data();
            pipelineLayoutCreateInfo.pSetLayouts = m_layout.data();
            pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)range.size();
            pipelineLayoutCreateInfo.setLayoutCount = m_layout.size();
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

            Loops::VkUtils::ErrorCheck(vkCreatePipelineLayout(m_vulkanContext.m_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));
        }

        // create pipeline
        {
            const VkFormat depthFormat = TextureManager::GetInstance()->GetBestFormat(TEXTURE_TYPE::DEPTH_STENCIL, false);

            VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
            pipelineRenderingCreateInfo.colorAttachmentCount = 0;
            pipelineRenderingCreateInfo.pColorAttachmentFormats = nullptr;
            pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
            pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

            // Create pipeline
            std::string vertSpvPath = std::string{ SPV_PATH } + "DepthVert.spv";
            std::string fragSpvPath = std::string{ SPV_PATH } + "DepthFrag.spv";

            VkPipelineShaderStageCreateInfo vertShaderStage, fragShaderStage;
            std::tie(m_vertexShaderModule, vertShaderStage) = Loops::VkUtils::CreateShaderModule(m_vulkanContext.m_logicalDevice, vertSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT);
            std::tie(m_fragmentShaderModule, fragShaderStage) = Loops::VkUtils::CreateShaderModule(m_vulkanContext.m_logicalDevice, fragSpvPath, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);

            VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
            pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            // Describe the vertex input, i.e. two vertex input attributes in our case:
            VkVertexInputBindingDescription vertexBindings{ 0, sizeof(Loops::Vertex), VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX };
            std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions
            {
                VkVertexInputAttributeDescription{0, 0, VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Loops::Vertex, Loops::Vertex::m_position)},
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

            Loops::VkUtils::ErrorCheck(vkCreateGraphicsPipelines(m_vulkanContext.m_logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
                nullptr, &m_pipeline));
        }

        // descriptor set resources (uniform and shader storage buffers)
        {
            // scene set 0
                // binding 0 camera
            {
                const uint16_t numUniforms = m_vulkanContext.m_maxFrameInFlights;

                // camera
                m_cameraUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_vulkanContext.m_physicalDevice, sizeof(CameraData));
                VkUtils::CreateBufferVma(m_cameraUniformDataSizePerFrame* numUniforms, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                    Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vkBuffer, m_cameraBuffer.m_vmaAllocation);
                vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vmaAllocation, &m_cameraUniformMemoryPointer);
                ASSERT_MSG(m_cameraUniformMemoryPointer != nullptr, "not mapped");

                m_sceneSet.resize(numUniforms);
                for (uint16_t i = 0; i < numUniforms; i++)
                {
                    VkDescriptorSetAllocateInfo setAllocInfo{};
                    setAllocInfo.descriptorPool = m_descriptorPool;
                    setAllocInfo.descriptorSetCount = 1;
                    setAllocInfo.pSetLayouts = &m_layout[SCENE_SET];
                    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

                    Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_vulkanContext.m_logicalDevice, &setAllocInfo, &m_sceneSet[i]));

                    {
                        VkDescriptorBufferInfo cameraBufferInfo{ m_cameraBuffer.m_vkBuffer, i * m_cameraUniformDataSizePerFrame, sizeof(CameraData) };

                        const VkWriteDescriptorSet writes[1]
                        {
                            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_sceneSet[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &cameraBufferInfo, nullptr},
                        };
                        vkUpdateDescriptorSets(m_vulkanContext.m_logicalDevice, 1, writes, 0, nullptr);
                    }
                }
            }

            // transform set 1
            {
                const uint16_t numUniforms = m_vulkanContext.m_maxFrameInFlights;

                const size_t dataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_vulkanContext.m_physicalDevice, sizeof(glm::mat4) * MAX_ENTITIES);
                m_transformUniformDataSizePerFrame = dataSizePerFrame;

                VkUtils::CreateBufferVma(dataSizePerFrame* numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
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
                        setAllocInfo.pSetLayouts = &m_layout[TRANSFORM_SET];
                        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

                        Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_vulkanContext.m_logicalDevice, &setAllocInfo, &m_transformSets[i]));

                        VkDescriptorBufferInfo bufferInfo{ m_transformBuffer.m_vkBuffer, i * dataSizePerFrame, dataSizePerFrame };
                        const VkWriteDescriptorSet writes
                        {
                            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_transformSets[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr
                        };
                        vkUpdateDescriptorSets(m_vulkanContext.m_logicalDevice, 1, &writes, 0, nullptr);
                    }
                }
            }
        }
    }

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

    VkUtils::ErrorCheck(vkCreateSampler(m_vulkanContext.m_logicalDevice, &samplerInfo, nullptr, &m_shadowSampler));
}

Loops::PointLight* Loops::LightManager::GetNewPointLight()
{
    auto index = m_pointLightCount++;
    Loops::ASSERT_MSG_DEBUG(m_pointLightCount < s_maxPointLights, "Out of range");
    return &m_pointLights[index];
}

Loops::DirectionalLight* Loops::LightManager::GetDirectionalLight()
{
    return &m_directionalLight;
}

const VkDescriptorSetLayoutBinding& Loops::LightManager::GetLightDataBinding() const
{
    return m_lightDataBinding;
}

const VkDescriptorSetLayoutBinding& Loops::LightManager::GetPointShadowMapBinding() const
{
    return m_pointShadowMapBinding;
}

const VkDescriptorSetLayoutBinding& Loops::LightManager::GetDirectionalShadowMapBinding() const
{
    return m_directionalShadowMapBinding;
}

const std::vector<Loops::LightUniform>& Loops::LightManager::GetLightUniformArray() const
{
    return m_lightUniformArray;
}

const VkImageView& Loops::LightManager::GetDirectionalLightShadowMap(uint32_t frameInFlight) const
{
    return m_depthTargets[frameInFlight].m_vkImageView;
}

const VkSampler& Loops::LightManager::GetShadowSampler() const
{
    return m_shadowSampler;
}

void Loops::LightManager::Update(const uint32_t& frameInFlight,
    const VkSemaphore& timelineSem, uint64_t signalValue,
    std::optional<uint64_t> waitValue, const Loops::RenderData& renderData,
    const Loops::SceneManager& sceneManager)
{
    // set 0 binding 0 camera
    {
        float nearPlane = 1.0f, farPlane = 50.5f;
        glm::mat4 orthoProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, nearPlane, farPlane);

        glm::vec3 position{ m_lightTransforms[0][3]};
        glm::mat4 view = glm::lookAt(position, glm::normalize(position + glm::vec3(m_lightTransforms[0][2])), glm::vec3(0.0f, 1.0f, 0.0f));

        CameraData uniform{ view, orthoProjection, glm::vec4{} };
        ASSERT_MSG(m_cameraUniformMemoryPointer != nullptr, "not yet mapped");
        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_cameraUniformMemoryPointer) + m_cameraUniformDataSizePerFrame * frameInFlight), &uniform, sizeof(CameraData));
    }

    // set 1 binding 0 transform array
    {
        ASSERT_MSG(m_transformUniformMemoryPointer != nullptr, "not yet mapped");
        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_transformUniformMemoryPointer) + m_transformUniformDataSizePerFrame * frameInFlight), renderData.m_modelMats, sizeof(glm::mat4) * renderData.m_drawableCount);
    }

    // Build Command Buffers
    {
        VkViewport viewport = { 0.0f, static_cast<float>(SHADOWMAP_HEIGHT), static_cast<float>(SHADOWMAP_WIDTH), -static_cast<float>(SHADOWMAP_HEIGHT), 0.0f, 1.0f };
        VkRect2D scissor = { {0, 0}, {SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT} };

        Loops::VkUtils::ErrorCheck(vkResetCommandBuffer(m_commandBuffers[frameInFlight], 0));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &beginInfo));

        auto RenderCommands = [this, &viewport, &scissor, &renderData, &sceneManager](uint32_t frameInFlight)
            {
                VkCommandBuffer& commandBuffer = m_commandBuffers[frameInFlight];

                vkCmdBeginRendering(commandBuffer, &m_renderingInfo[frameInFlight]);
                {
                    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                    // Scene set 0
                    // Transform set 1
                    VkDescriptorSet set[2]
                    { 
                        m_sceneSet[frameInFlight],
                        m_transformSets[frameInFlight]
                    };

                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

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

                    int boundVertexBuffer = -1, boundIndexBuffer = -1;
                    for (uint32_t i = 0; i < renderData.m_drawableCount; i++)
                    {
                        const Loops::Drawable& drawable = renderData.m_drawables[i];

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
                        uint32_t matIndex = drawable.m_matrixIndex;
                        VkPushConstantsInfo info{};
                        info.layout = m_pipelineLayout;
                        info.offset = 0;
                        info.pValues = &matIndex;
                        info.size = sizeof(matIndex);
                        info.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                        info.sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO;
                        vkCmdPushConstants2(m_commandBuffers[frameInFlight], &info);

                        // Launch draw
                        for (uint32_t i = 0; i < drawable.m_numOfViews; i++)
                        {
                            const Loops::MeshView& meshView = renderData.m_meshViews[drawable.m_viewStartIndex + i];
                            uint32_t numIndicies = meshView.m_indexCount;
                            uint32_t firstIndex = meshView.m_firstIndex;
                            vkCmdDrawIndexed(commandBuffer, numIndicies, 1, firstIndex, 0, 0);
                        }
                    }
                }
                vkCmdEndRendering(m_commandBuffers[frameInFlight]);
            };

        if (m_depthTargetLayouts[frameInFlight] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            VkImageMemoryBarrier2 imgBarrier{};
            imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            imgBarrier.image = m_depthTargets[frameInFlight].m_vkImage;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgBarrier.pNext = nullptr;
            imgBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            imgBarrier.subresourceRange.baseArrayLayer = 0;
            imgBarrier.subresourceRange.baseMipLevel = 0;
            imgBarrier.subresourceRange.layerCount = 1;
            imgBarrier.subresourceRange.levelCount = 1;

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &imgBarrier;
            dependencyInfo.pNext = nullptr;
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

            vkCmdPipelineBarrier2(m_commandBuffers[frameInFlight], &dependencyInfo);
            m_depthTargetLayouts[frameInFlight] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        RenderCommands(frameInFlight);

        if (m_depthTargetLayouts[frameInFlight] == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            VkImageMemoryBarrier2 imgBarrier{};
            imgBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            imgBarrier.image = m_depthTargets[frameInFlight].m_vkImage;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imgBarrier.pNext = nullptr;
            imgBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            imgBarrier.subresourceRange.baseArrayLayer = 0;
            imgBarrier.subresourceRange.baseMipLevel = 0;
            imgBarrier.subresourceRange.layerCount = 1;
            imgBarrier.subresourceRange.levelCount = 1;

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            dependencyInfo.imageMemoryBarrierCount = 1;
            dependencyInfo.pImageMemoryBarriers = &imgBarrier;
            dependencyInfo.pNext = nullptr;
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

            vkCmdPipelineBarrier2(m_commandBuffers[frameInFlight], &dependencyInfo);
            m_depthTargetLayouts[frameInFlight] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        Loops::VkUtils::ErrorCheck(vkEndCommandBuffer(m_commandBuffers[frameInFlight]));
    }

    // submit
    {
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

        Loops::VkUtils::ErrorCheck(vkQueueSubmit2(m_vulkanContext.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    }
}
