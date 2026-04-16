#include "pipelines/Pipeline.h"

Common::Pipeline::Pipeline(const PipelineInfo& info, std::shared_ptr<VulkanManager> pVulkanManager) : m_info(info), m_pVulkanManager(pVulkanManager)
{
    for (uint32_t i = 0; i < m_info.m_maxFrameInFlights; i++)
    {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        ErrorCheck(vkCreateSemaphore(m_info.m_device, &info, nullptr, &semaphore));
        m_swapchainImageAcquiredSemaphores.push_back(semaphore);
    }

    // Create in child class which has the details about the stages
    /*{
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_info.m_device));
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_info.m_device));
    }*/
}

Common::Pipeline::~Pipeline()
{
    for (auto& sem : m_swapchainImageAcquiredSemaphores)
        vkDestroySemaphore(m_info.m_device, sem, nullptr);
}
