#include "pipelines/TexturingPipeline.h"
#include "MaterialManager.h"
#include "TextureManager.h"
#include "LightManager.h"

Loops::Tasking::TexturingPipeline::TexturingPipeline(const VkUtils::VulkanContext* const vulkanContext,
    const std::unique_ptr<VulkanManager>& pVulkanManager,
    const std::unique_ptr<ImguiSystem>& imguiUtil,
    const Loops::MaterialManager* materialManager,
    const std::unique_ptr<SceneManager>& sceneManager) : Pipeline(vulkanContext),
    m_materialManager(materialManager), m_defaultColorTargets(pVulkanManager->GetDefaultColorImages()),
    m_defaultDepthTargets(pVulkanManager->GetDefaultDepthImages())

{
    {
        m_timelineSemaphores.emplace_back(std::make_unique<Loops::TimelineSemaphore>(m_vulkanContext->m_logicalDevice, uint32_t(TimelineStages::NUM_STAGES)));
        m_timelineSemaphores.emplace_back(std::make_unique<Loops::TimelineSemaphore>(m_vulkanContext->m_logicalDevice, uint32_t(TimelineStages::NUM_STAGES)));
    }

    VkFormat colorFormat{ TextureManager::GetInstance()->GetBestFormat(TEXTURE_TYPE::FBO, false)};

    /*mp_textureUnlitTask = std::make_unique<TextureUnlitTask>(taskInfo,
        pVulkanManager->GetDefaultColorImageView(),
        pVulkanManager->GetDefaultDepthImageView(),
        colorFormat, pVulkanManager->GetDepthFormat(),
        pVulkanManager->GetDefaultClearColor(),
        pVulkanManager->GetDefaultDepthClearValue());*/

    mp_phongShadingTask = std::make_unique<PhongShadingTask>(m_vulkanContext,
        pVulkanManager->GetDefaultColorImageView(),
        pVulkanManager->GetDefaultDepthImageView(),
        colorFormat, pVulkanManager->GetDepthFormat(),
        pVulkanManager->GetDefaultClearColor(),
        pVulkanManager->GetDefaultDepthClearValue(),
        m_materialManager, sceneManager->GetTransformDescriptorSetLayout(),
        sceneManager->GetCameraBuffer(),
        sceneManager->GetCameraDataSizePerFrame());
    /*
    const std::vector<VkDescriptorSet>& sceneSets,
            const std::vector<VkDescriptorSet>& transformSets,
            const VkDescriptorSetLayout& sceneSetLayout,
            const VkDescriptorSetLayout& transformSetLayout,
            const std::vector<VkImageView>& colorTargetViews,
            const std::vector<VkImageView>& depthTargetViews,
            const MaterialManager* pMaterialManager,
            const Loops::SceneManager* pSceneManager
    */
    mp_translucentEffect = std::make_unique<TranslucentEffect>(
        vulkanContext,
        mp_phongShadingTask->GetSceneSets(),
        sceneManager->GetTransformDescriptorSets(),
        mp_phongShadingTask->GetSceneSetLayout(),
        sceneManager->GetTransformDescriptorSetLayout(),
        pVulkanManager->GetDefaultColorImageView(),
        pVulkanManager->GetDefaultDepthImageView(),
        materialManager, sceneManager.get()
        );

    imguiUtil->CreateRenderingInfo();
}

Loops::Tasking::TexturingPipeline::~TexturingPipeline()
{
    mp_translucentEffect.reset();
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

        Loops::VkUtils::ErrorCheck(vkWaitSemaphores(m_vulkanContext->m_logicalDevice, &waitInfo, UINT64_MAX));
    }

    // Get the active swapchain index
    uint32_t activeSwapchainImageindex = vulkanManager->GetActiveSwapchainImageIndex(m_swapchainImageAcquiredSemaphores[currentFrameInFlight]);

    // trigger shadow pass
    {
        uint64_t currentValue{ 0 };
        vkGetSemaphoreCounterValue(m_vulkanContext->m_logicalDevice, m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(), &currentValue);
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::SHADOW_PASS_FINISHED);
        Loops::LightManager::GetInstance()->Update(
            currentFrameInFlight,
            m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt,
            sceneManager->GetRenderData(currentFrameInFlight),
            *sceneManager);
    }

    // Trigger textured unlit opaque task
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::SHADOW_PASS_FINISHED);
        /*mp_textureUnlitTask->Update(currentFrameInFlight,
            m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, std::nullopt, 
            sceneManager->GetRenderData(currentFrameInFlight),
            *sceneManager, m_materialManager->GetSceneMaterials());*/

        mp_phongShadingTask->Update(currentFrameInFlight,
            m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            signalValue, waitValue,
            sceneManager->GetRenderData(currentFrameInFlight),
            *sceneManager, m_materialManager->GetSceneMaterials(),
            sceneManager->GetTransformDescriptorSet(currentFrameInFlight));
    }

    uint64_t translucentSignalValue;
    // trigger translucent effect
    {
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        //uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::TRANSLUCENT_COPY_FINISHED);

        translucentSignalValue = mp_translucentEffect->Update(currentFrameInFlight,
            m_timelineSemaphores[currentFrameInFlight]->GetSemaphore(),
            waitValue,
            sceneManager->GetRenderData(currentFrameInFlight),
            sceneManager.get(),
            m_materialManager->GetSceneMaterials(),
            m_defaultColorTargets[currentFrameInFlight], m_defaultDepthTargets[currentFrameInFlight],
            mp_phongShadingTask->GetSceneSets()[currentFrameInFlight]
        );
    }

    // Trigger imgui
    {
        uint64_t signalValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::GUI_FINISHED);
        //uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::OPAQUE_FINISHED);
        uint64_t waitValue = m_timelineSemaphores[currentFrameInFlight]->GetTimelineValue(TimelineStages::TRANSLUCENT_FINISHED);
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
