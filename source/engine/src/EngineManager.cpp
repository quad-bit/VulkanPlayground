#include "EngineManager.h"
#include "Defines.h"
#include "Timer.h"

#include "Event.h"
#include "EventBus.h"

#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>


Loops::EngineManager::EngineManager(const Loops::EngineInfo& info, const AppCallbacks& callbacks) : m_appCallbacks(callbacks)
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    m_pTimer = std::make_unique<Timer>(60);
    Events::EventBus::GetInstance()->Init();

    m_pMemoryManager = std::make_unique<Loops::Memory::MemoryManager>();

    m_pSceneManager = std::make_unique<Loops::SceneManager>(info.m_gltfInfos, m_boundsManager, MAX_ENTITIES);

    m_pWindowManagerObj = std::make_unique<WindowManager>(info.m_screenSize.m_width, info.m_screenSize.m_height);
    m_pWindowManagerObj->Init();

    m_pVulkanManager = std::make_unique<VulkanManager>(info.m_screenSize.m_width, info.m_screenSize.m_height);
    auto dim = m_pVulkanManager->Init(m_pWindowManagerObj->glfwWindow);

    // VMA
    {
        Memory::MemoryManager::InitVMA(m_pVulkanManager->GetPhysicalDevice(), m_pVulkanManager->GetLogicalDevice(), m_pVulkanManager->GetInstance());
    }

    m_pInputManager = std::make_unique<IO::InputManager>(m_pWindowManagerObj->glfwWindow);

    ASSERT_MSG(std::get<0>(dim) == info.m_screenSize.m_width, "Mismatch");
    ASSERT_MSG(std::get<1>(dim) == info.m_screenSize.m_height, "Mismatch");

    m_pSceneManager->Initialise(m_pVulkanManager->GetLogicalDevice(), m_pVulkanManager->GetPhysicalDevice(), m_pVulkanManager->GetGraphicsQueue(),
        m_pVulkanManager->GetQueueFamilyIndex(), m_pVulkanManager->GetMaxFramesInFlight(), info.m_screenSize, info.m_designSize);

    m_maxFramesInFlight = m_pVulkanManager->GetMaxFramesInFlight();

    {
        auto func = [this](const Events::Event* event)
            {
                if ((static_cast<const Events::KeyInputEvent*>(event))->keyState == Events::KeyState::PRESSED && 
                    strcmp ((static_cast<const Events::KeyInputEvent*>(event))->keyname, "SPACE") == 0)
                {
                    //PLOGD << (static_cast<const Events::KeyInputEvent*>(event))->keyname;
                    ToggleGamePlayPauseState();
                }
            };

        Events::EventBus::GetInstance()->Subscribe<Events::KeyInputEvent>(func);


        /*Events::KeyInputEvent testEvent;
        testEvent.keyname = "SPACE";
        testEvent.keyState = Events::KeyState::RELEASED;

        Events::EventBus::GetInstance()->Publish<Events::KeyInputEvent>(&testEvent);*/
    }

    auto SetupWireframePipeline = [this](const Tasking::PipelineInfo& pipelineInfo)
        {
            m_pWireframePipeline = std::make_unique<Tasking::WireframePipeline>(pipelineInfo, m_pVulkanManager);
        };

    auto SetupBvhRenderPipeline = [this](const Tasking::PipelineInfo& pipelineInfo)
        {
            m_pBvhRenderPipeline = std::make_unique<Tasking::BvhRenderPipeline>(pipelineInfo, m_pVulkanManager);
        };

    auto SetupPipeline = [this, &info, &SetupWireframePipeline, &SetupBvhRenderPipeline](const std::vector<Tasking::PipelineType>& pipelineTypes)
        {
            m_activePipeline = info.m_pipelines[0];

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

            for (auto& type : info.m_pipelines)
            {
                switch (type)
                {
                    case Tasking::PipelineType::WIREFRAME:
                        SetupWireframePipeline(pipelineInfo);
                        break;

                    case Tasking::PipelineType::BVH_RENDER:
                        SetupBvhRenderPipeline(pipelineInfo);
                        break;

                    default :
                        break;
                }
            }
        };

    ASSERT_MSG(info.m_pipelines.size() > 0, "Pipeline info required");
    SetupPipeline(info.m_pipelines);

    // imgui
    m_pImguiUtil = std::make_unique<Loops::ImguiUtil >(m_pWindowManagerObj->glfwWindow, m_pVulkanManager->GetLogicalDevice(),
        m_pVulkanManager->GetPhysicalDevice(), m_pVulkanManager->GetGraphicsQueue(), m_pVulkanManager->GetQueueFamilyIndex(),
        m_pVulkanManager->GetMaxFramesInFlight(), info.m_screenSize.m_width, info.m_screenSize.m_height, m_pVulkanManager->GetDepthFormat(),
        VK_FORMAT_B8G8R8A8_UNORM, m_pVulkanManager->GetDefaultColorImageView());

    m_pImguiUtil->Init();

    //m_pEditor = std::make_unique<Loops::ImguiEditor>(*m_pImguiUtil, *m_pSceneManager, m_boundsManager);
    ImguiEditor::GetInstance()->Init(m_pImguiUtil.get(), m_pSceneManager.get(), &m_boundsManager);

    Init();
}

void Loops::EngineManager::Init()
{
    for (auto& onStartFunc : m_appCallbacks.m_Start)
    {
        onStartFunc(m_pSceneManager->m_world);
    }
}

void Loops::EngineManager::DeInit()
{
    if (m_pVulkanManager->AreTheQueuesIdle())
    {
        for (auto& onExit : m_appCallbacks.m_Exit)
        {
            onExit();
        }

        //if (m_pEditor)
        //{
        //    m_pEditor.reset();
        //    m_pEditor = nullptr;
        //}

        ImguiEditor::DeInit();

        if (m_pWireframePipeline)
        {
            m_pWireframePipeline.reset();
            m_pWireframePipeline = nullptr;
        }

        if (m_pBvhRenderPipeline)
        {
            m_pBvhRenderPipeline.reset();
            m_pBvhRenderPipeline = nullptr;
        }

        m_pSceneManager->DeInitialise();

        if (m_pImguiUtil)
        {
            m_pImguiUtil->Cleanup();
            m_pImguiUtil.reset();
            m_pImguiUtil = nullptr;
        }

        if (m_pInputManager)
        {
            m_pInputManager->DeInit();
            m_pInputManager.reset();
        }

        Memory::MemoryManager::DeInitVMA();

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

    Events::EventBus::GetInstance()->DeInit();
    delete Events::EventBus::GetInstance();

    if (m_pTimer)
    {
        m_pTimer.reset();
        m_pTimer = nullptr;
    }
}

const Loops::ImguiUtil& Loops::EngineManager::GetImguiUtil() const
{
    return *m_pImguiUtil.get();
}

const Loops::SceneManager& Loops::EngineManager::GetSceneManager() const
{
    return *m_pSceneManager;
}

const Loops::VulkanManager& Loops::EngineManager::GetVulkanManager() const
{
    return *m_pVulkanManager;
}

uint32_t Loops::EngineManager::GetMaxFrameInFlights() const
{
    return m_maxFramesInFlight;
}

void Loops::EngineManager::ToggleGamePlayPauseState()
{
    m_isGameplayPaused = m_isGameplayPaused ? false : true;
}

void Loops::EngineManager::Loop()
{
    while (m_pWindowManagerObj->Update())
    {
        m_pTimer->StartFrame();

        auto currentFrameInFlight = m_pVulkanManager->GetFrameInFlightIndex();

        m_pImguiUtil->NewFrame();

        if (!m_isGameplayPaused)
        {
            for (auto& onUpdate : m_appCallbacks.m_Update)
            {
                onUpdate(m_pTimer->GetDeltaTime());
            }

            m_pSceneManager->Update(currentFrameInFlight);
            m_boundsManager.Update(currentFrameInFlight, m_pSceneManager->m_world);
        }
        m_pSceneManager->Prepare(currentFrameInFlight);

        switch (m_activePipeline)
        {
        case Tasking::PipelineType::WIREFRAME:
            m_pWireframePipeline->Update(currentFrameInFlight, m_pSceneManager, m_boundsManager, m_pVulkanManager, m_pImguiUtil);
            break;

        case Tasking::PipelineType::BVH_RENDER:
            m_pBvhRenderPipeline->Update(currentFrameInFlight, m_pSceneManager, m_boundsManager, m_pVulkanManager, m_pImguiUtil);
            break;

        default:
            break;
        }
        m_pTimer->EndFrame();
    }
}
