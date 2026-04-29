#ifndef TIMELINE_SEMAPHORE_H
#define TIMELINE_SEMAPHORE_H

#include "Utils.h"

namespace Loops
{
    class TimelineSemaphore
    {
    private:
        VkSemaphore m_semaphore;
        uint32_t m_currentStage{ 0 };
        uint32_t m_maxStages{ 1 };
        uint64_t m_frameIndex = 0;

    public:
        const VkDevice& m_device;
        TimelineSemaphore() = delete;
        TimelineSemaphore(TimelineSemaphore const&) = delete;
        TimelineSemaphore& operator=(TimelineSemaphore const&) = delete;

        TimelineSemaphore(const VkDevice& device, uint32_t maxStages) : m_device(device), m_maxStages(maxStages)
        {
            VkSemaphoreTypeCreateInfoKHR typeCreateInfo{};
            typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
            typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
            typeCreateInfo.initialValue = 0;

            VkSemaphoreCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            info.pNext = &typeCreateInfo;
            Loops::VkUtils::ErrorCheck(vkCreateSemaphore(device, &info, nullptr, &m_semaphore));
        }

        ~TimelineSemaphore()
        {
            vkDestroySemaphore(m_device, m_semaphore, nullptr);
        }

        /*void SetTimelineStage(const TimelineStages& stage)
        {
            m_currentStage = stage;
        }*/

        void IncrementFrameIndex()
        {
            m_frameIndex++;
        }

        const VkSemaphore& GetSemaphore() const
        {
            return m_semaphore;
        }

        uint64_t GetFrameIndex() const
        {
            return m_frameIndex;
        }

        uint64_t GetTimelineValue(uint32_t stageValue) const
        {
            return m_frameIndex * (m_maxStages - 1) + stageValue;
        }
    };
}
#endif