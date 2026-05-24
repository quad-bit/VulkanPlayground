#include "pipelines/BvhRenderPipeline.h"
#include "BoundsManager.h"
#include <optional>

Loops::Tasking::BvhRenderPipeline::BvhRenderPipeline(const PipelineInfo& info, const std::unique_ptr<VulkanManager>& pVulkanManager) :
    Pipeline(info)
{
    {
        m_timelineSemaphores.emplace_back(std::make_unique<Loops::TimelineSemaphore>(m_info.m_device, uint32_t(TimelineStages::NUM_STAGES)));
        m_timelineSemaphores.emplace_back(std::make_unique<Loops::TimelineSemaphore>(m_info.m_device, uint32_t(TimelineStages::NUM_STAGES)));
    }

    GraphicsTaskInfo taskInfo = {};
    taskInfo.m_device = pVulkanManager->GetLogicalDevice();
    taskInfo.m_graphicsQueue = pVulkanManager->GetGraphicsQueue();
    taskInfo.m_maxFrameInFlights = pVulkanManager->GetMaxFramesInFlight();
    taskInfo.m_physicalDevice = pVulkanManager->GetPhysicalDevice();
    taskInfo.m_renderDimensions = info.m_designDimensions;
    taskInfo.m_queueFamilyIndex = info.m_graphicsQueueFamilyIndex;

    m_pColorUnlitTask = std::make_unique<Loops::Tasking::ColorUnlitTask>(taskInfo, pVulkanManager->GetDefaultColorImageView(), pVulkanManager->GetDefaultDepthImageView(),
        VK_FORMAT_B8G8R8A8_UNORM, pVulkanManager->GetDepthFormat());

    m_pBoundsRenderTask = std::make_unique<Loops::Tasking::BoundsRenderTask>(taskInfo, pVulkanManager->GetDefaultColorImageView(), VK_FORMAT_B8G8R8A8_UNORM);
}

Loops::Tasking::BvhRenderPipeline::~BvhRenderPipeline()
{
    m_pBoundsRenderTask.reset();
    m_pColorUnlitTask.reset();

    for (auto& sem : m_timelineSemaphores)
        sem.reset();
    m_timelineSemaphores.clear();
}

void Loops::Tasking::BvhRenderPipeline::Update(uint32_t currentFrameInFlight, const std::unique_ptr<Loops::SceneManager>& sceneManager, const BoundsManager& boundsManager,
    const std::unique_ptr<VulkanManager>& vulkanManager, const std::unique_ptr<Loops::ImguiUtil>& imguiUtil)
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

        Loops::VkUtils::ErrorCheck(vkWaitSemaphores(m_info.m_device, &waitInfo, UINT64_MAX));
    }

    // Trigger wireframe tasks
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        m_pColorUnlitTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt, sceneManager->GetRenderData(currentFrameInFlight), *sceneManager);
    }

    // Trigger bound render task
    {
        auto [primitiveBoundArray, numPrimitiveBounds] = boundsManager.GetPrimitiveBounds();
        auto [bvhNodeBoundArray, numBvhNodeBounds] = boundsManager.GetBvhNodeBounds();

        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::BOUND_RENDER_FINISHED);
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        auto& cameraData = sceneManager->GetRenderData(currentFrameInFlight).m_cameraData;
#if 0
        m_pBoundsRenderTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, waitValue, primitiveBoundArray, numPrimitiveBounds, sceneManager->GetMainCamera()->GetProjectionMat() * sceneManager->GetMainCamera()->GetViewMatrix());
#else
        m_pBoundsRenderTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(), signalValue, waitValue,
            primitiveBoundArray, numPrimitiveBounds, bvhNodeBoundArray, numBvhNodeBounds,
            cameraData.m_projectionMat * cameraData.m_viewMat);
#endif
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
