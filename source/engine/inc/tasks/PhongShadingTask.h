#ifndef PHONG_SHADING_TASK_H
#define PHONG_SHADING_TASK_H

#include "Task.h"
#include "SceneManager.h"
#include "memory/MemoryManager.h"

namespace Loops
{
    class MaterialManager;
}
namespace Loops::Tasking
{
    class PhongShadingTask : public GraphicsTask
    {
    private:
        struct alignas(16) PhongMaterialUniform
        {
            glm::vec4 m_color;
            int m_diffuseMapIndex;
            int m_normalMapIndex;
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

        std::vector<VkDescriptorSet> m_sceneSet;
        VulkanBuffer m_lightDataBuffer;
        const VulkanBuffer& m_cameraBuffer;

        std::vector<VkDescriptorSet> m_materialSet;
        VulkanBuffer m_materialBuffer;

        // as we are using one layout from texture manager
        // hence on destruction we dont want to destroy the borrowed one
        std::array<VkDescriptorSetLayout, 4> m_customLayout;

        std::vector<PhongMaterialUniform> m_materialArray;

        VkPipeline m_doubleSidedPipeline = VK_NULL_HANDLE;

        //const void* m_cameraUniformMemoryPointer = nullptr;
        void* m_lightUniformMemoryPointer = nullptr;
        void* m_materialUniformMemoryPointer = nullptr;
        size_t m_cameraUniformDataSizePerFrame = 0;
        size_t m_lightUniformDataSizePerFrame = 0;
        size_t m_materialUniformDataSizePerFrame = 0;

        void Init(std::optional<const VkClearColorValue> clearColorValue,
            std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
            const Loops::MaterialManager* pMaterialManager,
            const VkDescriptorSetLayout& transformSetLayout);

    public:
        PhongShadingTask(const GraphicsTaskInfo& info,
            const std::vector<VkImageView>& colorViews,
            const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat,
            std::optional<const VkClearColorValue> clearColorValue,
            std::optional<const VkClearDepthStencilValue> depthStencilClearValue,
            const Loops::MaterialManager* pMaterialManager,
            const VkDescriptorSetLayout& transformSetLayout,
            const Loops::VulkanBuffer& cameraBuffer,
            size_t cameraUniformDataSizePerFrame
            );

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
            uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager,
            const std::unordered_map<uint32_t, Loops::Material>& materials,
            const VkDescriptorSet& transformSet);

        // Case where submission is handled elsewhere
        void Update(VkCommandBuffer& commandBuffer, const uint32_t& frameInFlight,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager,
            std::optional<CameraData> secondaryCameraData);

        ~PhongShadingTask();
    };
}


#endif // !PHONG_SHADING_TASK_H
