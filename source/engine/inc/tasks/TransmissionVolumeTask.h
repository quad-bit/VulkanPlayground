#ifndef TRANSMISSION_VOLUME_TASK_H
#define TRANSMISSION_VOLUME_TASK_H

#include "Task.h"
#include "../RenderData.h"
#include "../SceneManager.h"

namespace Loops
{
    class MaterialManager;
}
namespace Loops::Tasking
{
    class TransmissionVolumeTask : public GraphicsTask
    {
    private:
        struct alignas(16) VolumeMaterialUniform
        {
            int m_metallicRoughnessTextureIndex = -1;
            int m_normalTextureIndex = -1;

            // KHR_materials_transmission
            int m_transmissionFactor = 1;

            // KHR_materials_volume
            //float m_thicknessFactor = 1.0f; or attenuationDistance
            float m_attenuationDistance = 1.0f;
            glm::vec4 m_attenuationColor{ 1.0f };
            glm::vec4 m_baseColor{ 1.0f };
        };

        static constexpr uint16_t SCENE_SET = 0;
        static constexpr uint16_t TRANSFORM_SET = 1;
        static constexpr uint16_t TEXTURE_SET = 2;// coming from textureManager
        static constexpr uint16_t MATERIAL_SET = 3;

        struct PushConsts
        {
            int transformIndex;
            int materialIndex;
            //int lightIndicies[3];
        };

        std::vector<VkDescriptorSet> m_sceneSets;
        std::vector<VkDescriptorSet> m_transformSets;

        std::vector<VkDescriptorSet> m_materialSet;
        VulkanBuffer m_materialBuffer;

        // as we are using one layout from texture manager
        // hence on destruction we dont want to destroy the borrowed one
        std::array<VkDescriptorSetLayout, 4> m_customLayout;

        std::vector<VolumeMaterialUniform> m_materialArray;

        void* m_materialUniformMemoryPointer = nullptr;
        size_t m_materialUniformDataSizePerFrame = 0; 

        std::vector<VkImageView> m_opaqueColorImageCopyViews;
        std::vector<VkImageView> m_opaqueDepthImageCopyViews;
        std::vector<VkImageView> m_backDepthImageViews;
        VkSampler m_sampler = VK_NULL_HANDLE;

        void Init(std::optional<const VkClearColorValue> clearColorValue,
            std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
            const Loops::MaterialManager* pMaterialManager,
            const VkDescriptorSetLayout& transformSetLayout);

    public:
        TransmissionVolumeTask(const VkUtils::VulkanContext * const vulkanContext,
            const std::vector<VkImageView>& colorViews,
            const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat,
            std::optional<const VkClearColorValue> clearColorValue,
            std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
            const Loops::MaterialManager* pMaterialManager,
            const VkDescriptorSetLayout& transformSetLayout,
            const Loops::VulkanBuffer& cameraBuffer,
            size_t cameraUniformDataSizePerFrame);

        TransmissionVolumeTask(const VkUtils::VulkanContext * const vulkanContext,
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
            bool createCommandBuffers
            );

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
            uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager,
            const std::unordered_map<uint32_t, Loops::Material>& materials,
            const VkDescriptorSet& transformSet,
            const VkDescriptorSet& sceneSet);

        // Case where submission is handled elsewhere
        /*void Update(VkCommandBuffer& commandBuffer, const uint32_t& frameInFlight,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager,
            std::optional<CameraData> secondaryCameraData);*/

        ~TransmissionVolumeTask();
    };
}
#endif // !TRANSMISSION_VOLUME_TASK_H
