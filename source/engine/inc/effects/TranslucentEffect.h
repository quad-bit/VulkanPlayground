#ifndef TRANSLUCENT_EFFECT
#define TRANSLUCENT_EFFECT

#include <vector>
#include "../tasks/DepthRenderTask.h"
#include "../tasks/TransmissionVolumeTask.h"
#include "../VulkanWrappers.h"
#include <taskflow/taskflow.hpp>

namespace Loops
{
    class TranslucentEffect
    {
    private:

        struct TaskData
        {
            VkImage m_opaqueColorImage;
            VkImage m_opaqueDepthImage;
        };

        struct FrameData
        {
            uint32_t m_currentFrameInFlight;
            uint64_t m_waitValue;
            VkSemaphore m_semaphore;
            VkDescriptorSet m_sceneSet;// from phong task
            std::vector<TaskData> m_taskDataList;
        }m_frameData;


        // a copy task required to create a mip of opaque color target
        std::vector<VulkanImage> m_opaquePassColorTargetCopy, m_opaquePassDepthTargetCopy;
        std::unique_ptr<Tasking::DepthRenderTask> mp_depthRenderTask;
        std::unique_ptr<Tasking::TransmissionVolumeTask> mp_transmissionVolumeTask;

        VkCommandPool m_commandPool = VK_NULL_HANDLE;
        //std::vector<VkCommandBuffer> m_commandBuffers;
        std::vector<VkCommandBuffer> m_opaqueGrabCommandBuffers;

        std::vector<VkDescriptorSet> m_sceneSets, m_transformSets;
        const VkUtils::VulkanContext * const m_vulkanContext = nullptr;

        std::vector<std::mutex> m_mutexList;
        std::vector<std::atomic_uint64_t> m_signalAtomics;
        tf::Executor m_executor;
        std::vector<tf::Taskflow> m_taskflows;

    public:
        TranslucentEffect(const Loops::VkUtils::VulkanContext* const vulkanContext,
            const std::vector<VkDescriptorSet>& sceneSets,
            const std::vector<VkDescriptorSet>& transformSets,
            const VkDescriptorSetLayout& sceneSetLayout,
            const VkDescriptorSetLayout& transformSetLayout,
            const std::vector<VkImageView>& colorTargetViews,
            const std::vector<VkImageView>& depthTargetViews,
            const MaterialManager* pMaterialManager,
            const Loops::SceneManager* pSceneManager
        );

        // returns signal value
        [[nodiscard]]
        uint64_t Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
            std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData,
            const Loops::SceneManager* sceneManager,
            const std::unordered_map<uint32_t, Loops::Material>& materials,
            VkImage& opaqueColorImage, VkImage& opaqueDepthImage,
            const VkDescriptorSet& sceneSet);

        ~TranslucentEffect();

    };
}

#endif // !TRANSLUCENT_EFFECT
