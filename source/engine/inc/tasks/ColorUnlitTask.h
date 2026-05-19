#ifndef COLOR_UNLIT_H
#define COLOR_UNLIT_H

#include "Task.h"
#include "SceneManager.h"
#include "memory/MemoryManager.h"

namespace Loops::Tasking
{
    class ColorUnlitTask : public GraphicsTask
    {
    private:
        const uint16_t CAMERA_SET = 0;
        const uint16_t TRANSFORM_SET = 1;

        std::vector<VkDescriptorSet> m_transformSets;
        VulkanBuffer m_transformBuffer;
        //VkDeviceMemory m_transformUniformMemory = VK_NULL_HANDLE;

        std::vector<VkDescriptorSet> m_viewSet;
        VulkanBuffer m_cameraBuffer;

        void* m_transformUniformMemoryPointer = nullptr;
        void* m_cameraUniformMemoryPointer = nullptr;
        size_t m_transformUniformDataSizePerFrame;
        size_t m_cameraUniformDataSizePerFrame;

        void Init();

    public:
        ColorUnlitTask(const GraphicsTaskInfo& info);

        ColorUnlitTask(const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData, const Loops::SceneManager& sceneManager);

        ~ColorUnlitTask();
    };
}

#endif // !COLOR_UNLIT_H
