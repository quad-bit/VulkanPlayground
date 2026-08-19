#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <mutex>
#include "Components.h"
#include "VulkanWrappers.h"
#include "RenderData.h"
#include "SceneManager.h"

#include <optional>

namespace Loops
{
    // Move this to utility and ask engine manager to create an object for this

    class LightManager
    {
    private:
        static constexpr uint32_t s_maxCubeShadowMaps = 2;
        static LightManager* s_instancePtr;
        static std::mutex s_mtx;
        static constexpr uint16_t s_maxPointLights = 3;

        static constexpr uint16_t SCENE_SET = 0;
        static constexpr uint16_t TRANSFORM_SET = 1;

        static constexpr uint16_t SHADOWMAP_WIDTH = 2048;
        static constexpr uint16_t SHADOWMAP_HEIGHT = 2048;

        LightManager();
        void DeInitPrivate();

        std::vector<PointLight> m_pointLights{ s_maxPointLights };
        uint16_t m_pointLightCount = 0;
        DirectionalLight m_directionalLight;

        // descriptor set 0 will contain the light related stuff maybe binding 1 and 2
        // as descriptor set light data
        // shadow maps for max 3 lights (maybe closest to the camera)

        VkDescriptorSetLayoutBinding m_lightDataBinding, m_directionalShadowMapBinding, m_pointShadowMapBinding;
        std::vector<LightUniform> m_lightUniformArray{ MAX_LIGHTS };
        std::vector<VulkanImage> m_depthTargets;
        std::vector<VkRenderingAttachmentInfo> m_renderAttachmentInfoList;
        std::vector<VkRenderingInfo> m_renderingInfo;

        VkUtils::VulkanContext m_vulkanContext;
        std::vector<VkDescriptorSetLayout> m_layout;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_commandBuffers;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkShaderModule m_vertexShaderModule = VK_NULL_HANDLE;
        VkShaderModule m_fragmentShaderModule = VK_NULL_HANDLE;

        // for shadow pass
        uint32_t m_cameraUniformDataSizePerFrame{ 0 };
        VulkanBuffer m_cameraBuffer;
        void* m_cameraUniformMemoryPointer{ nullptr };
        std::vector<VkDescriptorSet> m_sceneSet;

        uint32_t m_transformUniformDataSizePerFrame{ 0 };
        VulkanBuffer m_transformBuffer;
        void* m_transformUniformMemoryPointer{ nullptr };
        std::vector<VkDescriptorSet> m_transformSets;

        // change the layout from DEPTH_TARGET to SHADER_READ_ONLY
        // per frame in flight
        std::vector<VkImageLayout> m_depthTargetLayouts;
        std::vector<glm::mat4> m_lightTransforms;

        VkSampler m_shadowSampler = VK_NULL_HANDLE;

        // global resource

    public:
        static constexpr uint32_t MAX_LIGHTS = 4;
        static LightManager* GetInstance();
        static void DeInit();
        void Init(flecs::world& world, const VkUtils::VulkanContext& vulkanContext);
        PointLight* GetNewPointLight();
        DirectionalLight* GetDirectionalLight();

        const VkDescriptorSetLayoutBinding& GetLightDataBinding() const;
        const VkDescriptorSetLayoutBinding& GetPointShadowMapBinding() const;
        const VkDescriptorSetLayoutBinding& GetDirectionalShadowMapBinding() const;
        const std::vector<LightUniform>& GetLightUniformArray() const;
        const VkImageView& GetDirectionalLightShadowMap(uint32_t frameInFlight) const;
        const VkSampler& GetShadowSampler() const;

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
            uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager);
    };
}
#endif // !LIGHT_MANAGER_H
