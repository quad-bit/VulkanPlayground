#include "pipelines/WireframePipeline.h"
#include "BoundsManager.h"
#include <optional>

Loops::Tasking::WireframePipeline::WireframePipeline(const VkUtils::VulkanContext * const vulkanContext, const std::unique_ptr<VulkanManager>& pVulkanManager) :
    Pipeline(vulkanContext)
{
    {
        m_timelineSemaphores.emplace_back(std::make_unique<Loops::TimelineSemaphore>(m_vulkanContext->m_logicalDevice, uint32_t(TimelineStages::NUM_STAGES)));
        m_timelineSemaphores.emplace_back(std::make_unique<Loops::TimelineSemaphore>(m_vulkanContext->m_logicalDevice, uint32_t(TimelineStages::NUM_STAGES)));
    }

    m_pWireframeTask = std::make_unique<Loops::Tasking::WireFrameTask>(m_vulkanContext, pVulkanManager->GetDefaultColorImageView(), VK_FORMAT_B8G8R8A8_UNORM);

    m_pBoundsRenderTask = std::make_unique<Loops::Tasking::BoundsRenderTask>(m_vulkanContext, pVulkanManager->GetDefaultColorImageView(), pVulkanManager->GetDefaultDepthImageView(),
        pVulkanManager->GetSurfaceColorFormat(), pVulkanManager->GetDepthFormat());
}

Loops::Tasking::WireframePipeline::~WireframePipeline()
{
    m_pBoundsRenderTask.reset();
    m_pWireframeTask.reset();

    for (auto& sem : m_timelineSemaphores)
        sem.reset();
    m_timelineSemaphores.clear();
}

void Loops::Tasking::WireframePipeline::Update(uint32_t currentFrameInFlight, const std::unique_ptr<Loops::SceneManager>& sceneManager, const BoundsManager& boundsManager,
    const std::unique_ptr<VulkanManager>& vulkanManager, const std::unique_ptr<Loops::ImguiSystem>& imguiUtil)
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

        Loops::VkUtils::ErrorCheck(vkWaitSemaphores(m_vulkanContext->m_logicalDevice, &waitInfo, UINT64_MAX));
    }

    // Trigger wireframe tasks
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::WIREFRAME_FINISHED);
        m_pWireframeTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt, sceneManager->GetRenderData(currentFrameInFlight), *sceneManager);
    }

    // Trigger bound render task
    {
        auto [primitiveBoundArray, numPrimitiveBounds] = boundsManager.GetPrimitiveBounds();
        auto [primitiveNodeBoundArray, numPrimitiveNodeBounds] = boundsManager.GetBvhNodeBounds();

        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::BOUND_RENDER_FINISHED);
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::WIREFRAME_FINISHED);

        auto& cameraData = sceneManager->GetRenderData(currentFrameInFlight).m_cameraData;

    #if 0
        //m_pBoundsRenderTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            //signalValue, waitValue, primitiveBoundArray, numPrimitiveBounds, sceneManager->GetMainCamera()->GetProjectionMat() * sceneManager->GetMainCamera()->GetViewMatrix());
    #else
        m_pBoundsRenderTask->Update(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(), signalValue, waitValue,
            primitiveBoundArray, numPrimitiveBounds, primitiveNodeBoundArray, numPrimitiveNodeBounds,
            cameraData.m_projectionMat * cameraData.m_viewMat, sceneManager->GetRenderData(currentFrameInFlight), *sceneManager);
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
