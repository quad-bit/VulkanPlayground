#include "EngineManager.h"
#include "Defines.h"
#include "Timer.h"

#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>

Common::EngineManager::EngineManager(const Common::EngineInfo& info)
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    m_pTimer = std::make_unique<Timer>(60);
    m_pMemoryManager = std::make_unique<Common::Memory::MemoryManager>();

    m_pSceneManager = std::make_unique<Common::SceneManager>(info.m_gltfInfos, MAX_ENTITIES);

    m_pWindowManagerObj = std::make_unique<WindowManager>(info.m_screenSize.m_width, info.m_screenSize.m_height);
    m_pWindowManagerObj->Init();

    m_pVulkanManager = std::make_unique<VulkanManager>(info.m_screenSize.m_width, info.m_screenSize.m_height);
    auto dim = m_pVulkanManager->Init(m_pWindowManagerObj->glfwWindow);

    assert(std::get<0>(dim) == info.m_screenSize.m_width);
    assert(std::get<1>(dim) == info.m_screenSize.m_height);

    m_pSceneManager->Initialise(m_pVulkanManager->GetLogicalDevice(), m_pVulkanManager->GetPhysicalDevice(), m_pVulkanManager->GetGraphicsQueue(),
        m_pVulkanManager->GetQueueFamilyIndex(), m_pVulkanManager->GetMaxFramesInFlight(), info.m_screenSize, info.m_designSize);

    m_maxFramesInFlight = m_pVulkanManager->GetMaxFramesInFlight();

    // pipeline creation
    {
        Tasking::PipelineInfo pipelineInfo{};
        pipelineInfo.m_computeQueue = m_pVulkanManager->GetComputeQueue();
        pipelineInfo.m_computeQueueFamilyIndex = m_pVulkanManager->GetQueueFamilyIndex();
        pipelineInfo.m_device = m_pVulkanManager->GetLogicalDevice();
        pipelineInfo.m_graphicsQueue = m_pVulkanManager->GetGraphicsQueue();
        pipelineInfo.m_graphicsQueueFamilyIndex = m_pVulkanManager->GetQueueFamilyIndex();
        pipelineInfo.m_maxFrameInFlights = m_pVulkanManager->GetMaxFramesInFlight();
        pipelineInfo.m_physicalDevice = m_pVulkanManager->GetPhysicalDevice();
        pipelineInfo.m_screenDimensions = info.m_screenSize;
        pipelineInfo.m_designDimensions = info.m_designSize;

        m_pWireframePipeline = std::make_unique<Tasking::WireframePipeline>(pipelineInfo, m_pVulkanManager, m_pSceneManager->GetMainCamera());
    }

    // imgui
    m_pImguiUtil = std::make_unique<Common::ImguiUtil >(m_pWindowManagerObj->glfwWindow, m_pVulkanManager->GetLogicalDevice(),
        m_pVulkanManager->GetPhysicalDevice(), m_pVulkanManager->GetGraphicsQueue(), m_pVulkanManager->GetQueueFamilyIndex(),
        m_pVulkanManager->GetMaxFramesInFlight(), info.m_screenSize.m_width, info.m_screenSize.m_height, m_pVulkanManager->GetDepthFormat(),
        VK_FORMAT_B8G8R8A8_UNORM, m_pVulkanManager->GetDefaultColorImageView());

    m_pImguiUtil->Init();

    m_pEditor = std::make_unique<Common::ImguiEditor>(*m_pImguiUtil, *m_pSceneManager);
}

void Common::EngineManager::Init()
{

}

void Common::EngineManager::DeInit()
{
    if (m_pVulkanManager->AreTheQueuesIdle())
    {
        m_pSceneManager->DeInitialise();

        if (m_pEditor)
        {
            m_pEditor.reset();
            m_pEditor = nullptr;
        }

        if (m_pWireframePipeline)
        {
            m_pWireframePipeline.reset();
            m_pWireframePipeline = nullptr;
        }

        if (m_pImguiUtil)
        {
            m_pImguiUtil->Cleanup();
            m_pImguiUtil.reset();
            m_pImguiUtil = nullptr;
        }

        m_pVulkanManager->DeInit();
        m_pWindowManagerObj->DeInit();

        m_pSceneManager.reset();
        m_pSceneManager = nullptr;
    }

    if (m_pMemoryManager)
    {
        m_pMemoryManager.reset();
        m_pMemoryManager = nullptr;
    }

    if (m_pTimer)
    {
        m_pTimer.reset();
        m_pTimer = nullptr;
    }
}

const Common::ImguiUtil& Common::EngineManager::GetImguiUtil() const
{
    return *m_pImguiUtil.get();
}

const Common::SceneManager& Common::EngineManager::GetSceneManager() const
{
    return *m_pSceneManager;
}

const Common::VulkanManager& Common::EngineManager::GetVulkanManager() const
{
    return *m_pVulkanManager;
}

uint32_t Common::EngineManager::GetMaxFrameInFlights() const
{
    return m_maxFramesInFlight;
}

void Common::EngineManager::Loop()
{
    while (m_pWindowManagerObj->Update())
    {
        m_pTimer->StartFrame();

        auto currentFrameInFlight = m_pVulkanManager->GetFrameInFlightIndex();

        m_pImguiUtil->NewFrame();

        m_pSceneManager->Update(currentFrameInFlight);

        m_pSceneManager->Prepare(currentFrameInFlight);

        m_pWireframePipeline->Update(currentFrameInFlight, m_pSceneManager, m_pVulkanManager, m_pImguiUtil);
        m_pTimer->EndFrame();
    }
}
