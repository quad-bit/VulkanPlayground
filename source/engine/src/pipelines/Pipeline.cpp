#include "pipelines/Pipeline.h"

Loops::Tasking::Pipeline::Pipeline(const VkUtils::VulkanContext * const  context) : m_vulkanContext(context)
{
    for (uint32_t i = 0; i < m_vulkanContext->m_maxFrameInFlights; i++)
    {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        Loops::VkUtils::ErrorCheck(vkCreateSemaphore(m_vulkanContext->m_logicalDevice, &info, nullptr, &semaphore));
        m_swapchainImageAcquiredSemaphores.push_back(semaphore);
    }

    // Create in child class which has the details about the stages
    /*{
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_vulkanContext->m_logicalDevice));
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_vulkanContext->m_logicalDevice));
    }*/
}

Loops::Tasking::Pipeline::~Pipeline()
{
    for (auto& sem : m_swapchainImageAcquiredSemaphores)
        vkDestroySemaphore(m_vulkanContext->m_logicalDevice, sem, nullptr);
}
