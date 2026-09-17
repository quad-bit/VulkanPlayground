#ifndef DEPTH_RENDER_TASK_H
#define DEPTH_RENDER_TASK_H

#include "Task.h"
#include "../RenderData.h"
#include "../SceneManager.h"
#include <mutex>

namespace Loops::Tasking
{
    class DepthRenderTask : public GraphicsTask
    {
    private:

        static constexpr uint16_t SCENE_SET = 0;
        static constexpr uint16_t TRANSFORM_SET = 1;

        struct PushConsts
        {
            int transformIndex;
        };

        // as we are using one layout from texture manager
        // hence on destruction we dont want to destroy the borrowed one
        std::array<VkDescriptorSetLayout, 2> m_customLayout;
        std::vector<VkDescriptorSet> m_sceneSet, m_transformSet;
        std::vector<std::pair<Loops::EFFECT_TYPE, Loops::TECHNIQUE_TYPE>> m_targetPasses;

        void Init(bool allocateCommandBuffers,
            const VkClearDepthStencilValue& depthStencilClearValue,
            VkCullModeFlags cullMode);

    public:
        DepthRenderTask(const VkUtils::VulkanContext * const vulkanContext,
            const VkDescriptorSetLayout& transformSetLayout,
            const VkDescriptorSetLayout& sceneSetLayout,
            const std::vector<VkDescriptorSet>& sceneSet,
            const std::vector<VkDescriptorSet>& transformSet,
            const VkFormat& depthFormat,
            uint32_t numTargets,
            const VkClearDepthStencilValue depthStencilClearValue,
            std::vector<std::pair<Loops::EFFECT_TYPE, Loops::TECHNIQUE_TYPE>> targetPasses,
            bool allocateCommandBuffers,
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
            std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData,
            const Loops::SceneManager* sceneManager,
            const std::unordered_map<uint32_t, Loops::Material>& materials,
            const VkDescriptorSet& transformSet,
            std::mutex& mutex, std::atomic_uint64_t& signalAtomic
        );

        // Case where submission is handled elsewhere
        void Update(VkCommandBuffer& commandBuffer, const uint32_t& frameInFlight,
            const Loops::RenderData& renderData,
            const Loops::SceneManager& sceneManager);

        std::vector<VkImageView> GetDepthTargetViews() const;

        ~DepthRenderTask();
    };
}
#endif // !DEPTH_RENDER_TASK_H
