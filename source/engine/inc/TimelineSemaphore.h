#pragma once
#include "Utils.h"

// yet to figure out app specific override
#ifndef TIMELINE_STAGE_OVERRIDE

enum TimelineStages
{
    UNINITIALIZED = 0,
    COMPUTE_FINISHED = 1,
    GRAPHICS_FINISHED = 2,
    GUI_FINISHED = 3,
    SAFE_TO_PRESENT = 4,
    NUM_STAGES = 5
};

#endif // TIMELINE_STAGE_OVERRIDE


class TimelineSemaphore
{
private:
    VkSemaphore m_semaphore;
    TimelineStages m_currentStage{ TimelineStages::UNINITIALIZED };
    uint64_t m_frameIndex = 0;

public:
    const VkDevice& m_device;
    TimelineSemaphore() = delete;
    TimelineSemaphore(TimelineSemaphore const&) = delete;
    TimelineSemaphore& operator=(TimelineSemaphore const&) = delete;

    TimelineSemaphore(const VkDevice& device) : m_device(device)
    {
        VkSemaphoreTypeCreateInfoKHR typeCreateInfo{};
        typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
        typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
        typeCreateInfo.initialValue = 0;

        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &typeCreateInfo;
        ErrorCheck(vkCreateSemaphore(device, &info, nullptr, &m_semaphore));
    }

    ~TimelineSemaphore()
    {
        vkDestroySemaphore(m_device, m_semaphore, nullptr);
    }

    void SetTimelineStage(const TimelineStages& stage)
    {
        m_currentStage = stage;
    }

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

    uint64_t GetTimelineValue(const TimelineStages& stage)
    {
        return m_frameIndex * (TimelineStages::NUM_STAGES - 1) + stage;
    }

};
