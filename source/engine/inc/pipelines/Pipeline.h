#ifndef PIPELINE_H
#define PIPELINE_H

#include "Utils.h"
#include "TimelineSemaphore.h"
#include "VulkanManager.h"
#include <memory>

namespace Common::Tasking
{
    struct PipelineInfo
    {
        VkDevice m_device;
        VkPhysicalDevice m_physicalDevice;
        Dimension m_screenDimensions;
        Dimension m_designDimensions;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_computeQueue = VK_NULL_HANDLE;
        uint32_t m_graphicsQueueFamilyIndex;
        uint32_t m_computeQueueFamilyIndex;
        uint32_t m_maxFrameInFlights;
    };

    class Pipeline
    {
    private:

    protected:
        std::vector<VkSemaphore> m_swapchainImageAcquiredSemaphores;
        std::vector<std::unique_ptr<TimelineSemaphore>> m_timelineSemaphores;

        PipelineInfo m_info;

    public:
        Pipeline(const PipelineInfo& info);
        ~Pipeline();
    };
}

#endif // !PIPELINE_H
