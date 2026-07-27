#ifndef TEXTURE_UNLIT_TASK_H
#define TEXTURE_UNLIT_TASK_H

#include "Task.h"
#include "SceneManager.h"
#include "memory/MemoryManager.h"

namespace Loops::Tasking
{
    class TextureUnlitTask : public GraphicsTask
    {
    private:

        const uint16_t CAMERA_SET = 0;
        const uint16_t TRANSFORM_SET = 1;
        const uint16_t TEXTURE_SET = 2;// coming from textureManager

        std::vector<VkDescriptorSet> m_transformSets;
        VulkanBuffer m_transformBuffer;

        std::vector<VkDescriptorSet> m_viewSet;
        VulkanBuffer m_cameraBuffer;

        // as we are using one layout from texture manager
        // hence on destruction we dont want to destroy the borrowed one
        std::array<VkDescriptorSetLayout, 3> m_customLayout;


        void* m_transformUniformMemoryPointer = nullptr;
        void* m_cameraUniformMemoryPointer = nullptr;
        size_t m_transformUniformDataSizePerFrame = 0;
        size_t m_cameraUniformDataSizePerFrame = 0;

        void Init(std::optional<const VkClearColorValue> clearColorValue,
            std::optional<const VkClearDepthStencilValue> depthStencilClearValue);

    public:
        TextureUnlitTask(const GraphicsTaskInfo& info,
            const std::vector<VkImageView>& colorViews,
            const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat,
            std::optional<const VkClearColorValue> clearColorValue,
            std::optional<const VkClearDepthStencilValue> depthStencilClearValue);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
            uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager,
            const std::unordered_map<uint32_t, Loops::Material>& materials);

        // Case where submission is handled elsewhere
        void Update(VkCommandBuffer& commandBuffer, const uint32_t& frameInFlight,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager,
            std::optional<CameraData> secondaryCameraData);

        ~TextureUnlitTask();
    };
}


#endif // !TEXTURE_UNLIT_TASK_H
