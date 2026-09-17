#ifndef PIPELINE_H
#define PIPELINE_H

#include "Utils.h"
#include "Defines.h"
#include "TimelineSemaphore.h"
#include "VulkanManager.h"
#include <memory>

namespace Loops::Tasking
{
    enum class PipelineType
    {
        WIREFRAME,
        BVH_RENDER,
        TEXTURED
    };

    class Pipeline
    {
    private:

    protected:
        std::vector<VkSemaphore> m_swapchainImageAcquiredSemaphores;
        std::vector<std::unique_ptr<TimelineSemaphore>> m_timelineSemaphores;

        const VkUtils::VulkanContext * const m_vulkanContext = nullptr;

    public:
        Pipeline(const VkUtils::VulkanContext * const vulkanContext);
        ~Pipeline();
    };
}

#endif // !PIPELINE_H
