#include "pipelines/WireframePipeline.h"
#include <optional>

Common::WireframePipeline::WireframePipeline(const PipelineInfo& info, std::shared_ptr<VulkanManager> pVulkanManager, std::unique_ptr<Common::Camera>& pCamera) :
    Pipeline(info, pVulkanManager)
{
    {
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_info.m_device, uint32_t(TimelineStages::NUM_STAGES)));
        m_timelineSemaphores.emplace_back(std::make_unique<TimelineSemaphore>(m_info.m_device, uint32_t(TimelineStages::NUM_STAGES)));
    }

    GraphicsTaskInfo taskInfo = {};
    taskInfo.m_device = pVulkanManager->GetLogicalDevice();
    taskInfo.m_graphicsQueue = pVulkanManager->GetGraphicsQueue();
    taskInfo.m_maxFrameInFlights = pVulkanManager->GetMaxFramesInFlight();
    taskInfo.m_physicalDevice = pVulkanManager->GetPhysicalDevice();
    taskInfo.m_renderDimensions = info.m_designDimensions;
    taskInfo.m_queueFamilyIndex = info.m_graphicsQueueFamilyIndex;

    m_pWireframeTask = std::make_unique<Common::WireFrameTask>(taskInfo, m_pVulkanManager->GetDefaultColorImageView(),
        m_pVulkanManager->GetDefaultDepthImageView(), VK_FORMAT_B8G8R8A8_UNORM, m_pVulkanManager->GetDepthFormat(), pCamera);
}

Common::WireframePipeline::~WireframePipeline()
{
    m_pWireframeTask.reset();

    for (auto& sem : m_timelineSemaphores)
        sem.reset();
    m_timelineSemaphores.clear();
}

void Common::WireframePipeline::Update(uint32_t currentFrameInFlight, std::shared_ptr<Common::SceneManager> sceneManager,
    std::shared_ptr < VulkanManager> vulkanManager, const std::shared_ptr<Common::ImguiUtil>& imguiUtil)
{
    if (m_timelineSemaphores[currentFrameInFlight]->GetFrameIndex() > 0)
    {
        // wait for previous frame's (corresponding frameInFlight) presentation to complete
        // this acts as a fence

        uint64_t value = (m_timelineSemaphores[currentFrameInFlight]->GetFrameIndex() - 1) * (TimelineStages::NUM_STAGES - 1) + TimelineStages::SAFE_TO_PRESENT;

        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.pSemaphores = &m_timelineSemaphores[currentFrameInFlight]->GetSemaphore();
        waitInfo.pValues = &value;
        waitInfo.semaphoreCount = 1;
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;

        ErrorCheck(vkWaitSemaphores(m_info.m_device, &waitInfo, UINT64_MAX));
    }

    // Trigger graphics tasks
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::WIREFRAME_FINISHED);
        m_pWireframeTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt, sceneManager->GetRenderData(currentFrameInFlight), *sceneManager);
    }

    // Trigger imgui
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::UNINITIALIZED);
        imguiUtil->Render(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(), signalValue, waitValue);
    }

    // Get the active swapchain index
    uint32_t activeSwapchainImageindex = vulkanManager->GetActiveSwapchainImageIndex(m_swapchainImageAcquiredSemaphores[currentFrameInFlight]);

    // End the frame (increments index counters)
    {
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::SAFE_TO_PRESENT);
        vulkanManager->CopyAndPresent(vulkanManager->GetDefaultColorImages()[currentFrameInFlight], m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            m_swapchainImageAcquiredSemaphores[currentFrameInFlight], waitValue, signalValue);
    }

    m_timelineSemaphores[currentFrameInFlight]->IncrementFrameIndex();
}
