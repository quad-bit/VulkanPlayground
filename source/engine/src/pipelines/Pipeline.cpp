#include "pipelines/Pipeline.h"

Loops::Tasking::Pipeline::Pipeline(const PipelineInfo& info) : m_info(info)
{
    for (uint32_t i = 0; i < m_info.m_maxFrameInFlights; i++)
    {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        Loops::VkUtils::ErrorCheck(vkCreateSemaphore(m_info.m_device, &info, nullptr, &semaphore));
        m_swapchainImageAcquiredSemaphores.push_back(semaphore);
    }

    // Create in child class which has the details about the stages
    /*{
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_info.m_device));
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_info.m_device));
    }*/
}

Loops::Tasking::Pipeline::~Pipeline()
{
    for (auto& sem : m_swapchainImageAcquiredSemaphores)
        vkDestroySemaphore(m_info.m_device, sem, nullptr);
}
