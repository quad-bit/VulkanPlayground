#include "pipelines/TexturingPipeline.h"
#include "MaterialManager.h"
#include "TextureManager.h"

Loops::Tasking::TexturingPipeline::TexturingPipeline(const PipelineInfo& info,
    const std::unique_ptr<VulkanManager>& pVulkanManager,
    const std::unique_ptr<ImguiSystem>& imguiUtil,
    const Loops::MaterialManager* materialManager) : Pipeline(info),
    m_materialManager(materialManager)
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

    VkFormat colorFormat{ TextureManager::GetInstance()->GetBestFormat(TEXTURE_TYPE::FBO, false)};

    /*mp_textureUnlitTask = std::make_unique<TextureUnlitTask>(taskInfo,
        pVulkanManager->GetDefaultColorImageView(),
        pVulkanManager->GetDefaultDepthImageView(),
        colorFormat, pVulkanManager->GetDepthFormat(),
        pVulkanManager->GetDefaultClearColor(),
        pVulkanManager->GetDefaultDepthClearValue());*/

    mp_phongShadingTask = std::make_unique<PhongShadingTask>(taskInfo,
        pVulkanManager->GetDefaultColorImageView(),
        pVulkanManager->GetDefaultDepthImageView(),
        colorFormat, pVulkanManager->GetDepthFormat(),
        pVulkanManager->GetDefaultClearColor(),
        pVulkanManager->GetDefaultDepthClearValue(),
        m_materialManager);

    imguiUtil->CreateRenderingInfo();
}

Loops::Tasking::TexturingPipeline::~TexturingPipeline()
{
    //mp_textureUnlitTask.reset();
    mp_phongShadingTask.reset();

    for (auto& sem : m_timelineSemaphores)
        sem.reset();
    m_timelineSemaphores.clear();
}

void Loops::Tasking::TexturingPipeline::Update(uint32_t currentFrameInFlight,
    const std::unique_ptr<SceneManager>& sceneManager,
    const BoundsManager& boundsManager,
    const std::unique_ptr<VulkanManager>& vulkanManager,
    const std::unique_ptr<ImguiSystem>& imguiUtil)
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

    // Get the active swapchain index
    uint32_t activeSwapchainImageindex = vulkanManager->GetActiveSwapchainImageIndex(m_swapchainImageAcquiredSemaphores[currentFrameInFlight]);

    // Trigger textured unlit opaque task
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        /*mp_textureUnlitTask->Update(currentFrameInFlight,
            m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt, 
            sceneManager->GetRenderData(currentFrameInFlight),
            *sceneManager, m_materialManager->GetSceneMaterials());*/

        mp_phongShadingTask->Update(currentFrameInFlight,
            m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt,
            sceneManager->GetRenderData(currentFrameInFlight),
            *sceneManager, m_materialManager->GetSceneMaterials());
    }

    // Trigger imgui
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        imguiUtil->Render(currentFrameInFlight, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(), signalValue, waitValue);
    }

    // End the frame (increments index counters)
    {
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::SAFE_TO_PRESENT);
        vulkanManager->CopyAndPresent(vulkanManager->GetDefaultColorImages()[currentFrameInFlight], m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            m_swapchainImageAcquiredSemaphores[currentFrameInFlight], waitValue, signalValue);
    }

    m_timelineSemaphores[currentFrameInFlight]->IncrementFrameIndex();
}
