#include "EngineManager.h"
#include "Defines.h"
#include "Timer.h"

#include "Event.h"
#include "EventBus.h"
#include "TextureManager.h"

#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>

Loops::EngineManager::EngineManager(const Loops::EngineInfo& info, const AppCallbacks& callbacks) : m_appCallbacks(callbacks)
{
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::debug, &consoleAppender);

    mp_Timer = std::make_unique<Timer>(60);
    Memory::MemoryManager::GetInstance()->Init();
    Events::EventBus::GetInstance()->Init();

    //mp_MemoryManager = std::make_unique<Loops::Memory::MemoryManager>();

    mp_WindowManagerObj = std::make_unique<WindowManager>(info.m_screenSize.m_width, info.m_screenSize.m_height, info.m_enableFullScreen);
    mp_WindowManagerObj->Init();

    mp_VulkanManager = std::make_unique<VulkanManager>(info.m_screenSize.m_width, info.m_screenSize.m_height);
    auto dim = mp_VulkanManager->Init(mp_WindowManagerObj->glfwWindow);

    Memory::MemoryManager::GetInstance()->InitVMA(mp_VulkanManager->GetPhysicalDevice(), mp_VulkanManager->GetLogicalDevice(), mp_VulkanManager->GetInstance());

    TextureManager::GetInstance()->Init(mp_VulkanManager->GetPhysicalDevice(),
        mp_VulkanManager->GetLogicalDevice(),
        mp_VulkanManager->GetGraphicsQueue(), mp_VulkanManager->GetQueueFamilyIndex());

    mp_InputManager = std::make_unique<IO::InputManager>(mp_WindowManagerObj->glfwWindow);

    ASSERT_MSG(std::get<0>(dim) == info.m_screenSize.m_width, "Mismatch");
    ASSERT_MSG(std::get<1>(dim) == info.m_screenSize.m_height, "Mismatch");

    m_maxFramesInFlight = mp_VulkanManager->GetMaxFramesInFlight();

    // imgui
    mp_ImguiSystem = std::make_unique<Loops::ImguiSystem>(mp_WindowManagerObj->glfwWindow, mp_VulkanManager.get(), mp_VulkanManager->GetLogicalDevice(),
        mp_VulkanManager->GetPhysicalDevice(), mp_VulkanManager->GetGraphicsQueue(), mp_VulkanManager->GetQueueFamilyIndex(),
        mp_VulkanManager->GetMaxFramesInFlight(), info.m_screenSize.m_width, info.m_screenSize.m_height,
        /*mp_VulkanManager->GetSurfaceColorFormat()*/ VK_FORMAT_B8G8R8A8_UNORM, mp_VulkanManager->GetDefaultColorImageView());

    mp_materialManager = std::make_unique<Loops::MaterialManager>();
    mp_SceneManager = std::make_unique<Loops::SceneManager>(info.m_gltfInfos,
        m_boundsManager, MAX_ENTITIES, mp_materialManager.get());
    mp_SceneManager->Initialise(mp_VulkanManager->GetLogicalDevice(),
        mp_VulkanManager->GetPhysicalDevice(),
        mp_VulkanManager->GetGraphicsQueue(),
        mp_VulkanManager->GetQueueFamilyIndex(),
        mp_VulkanManager->GetMaxFramesInFlight(),
        info.m_screenSize, info.m_designSize);

    ImguiEditor::GetInstance()->Init(mp_ImguiSystem.get(), mp_SceneManager.get(), &m_boundsManager);

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
            mp_WireframePipeline = std::make_unique<Tasking::WireframePipeline>(pipelineInfo, mp_VulkanManager);
        };

    auto SetupBvhRenderPipeline = [this](const Tasking::PipelineInfo& pipelineInfo)
        {
            mp_BvhRenderPipeline = std::make_unique<Tasking::BvhRenderPipeline>(pipelineInfo, mp_VulkanManager, mp_ImguiSystem);
        };

    auto SetupPipeline = [this, &info, &SetupWireframePipeline, &SetupBvhRenderPipeline](const std::vector<Tasking::PipelineType>& pipelineTypes)
        {
            m_activePipeline = info.m_pipelines[0];

            Tasking::PipelineInfo pipelineInfo{};
            pipelineInfo.m_computeQueue = mp_VulkanManager->GetComputeQueue();
            pipelineInfo.m_computeQueueFamilyIndex = mp_VulkanManager->GetQueueFamilyIndex();
            pipelineInfo.m_device = mp_VulkanManager->GetLogicalDevice();
            pipelineInfo.m_graphicsQueue = mp_VulkanManager->GetGraphicsQueue();
            pipelineInfo.m_graphicsQueueFamilyIndex = mp_VulkanManager->GetQueueFamilyIndex();
            pipelineInfo.m_maxFrameInFlights = mp_VulkanManager->GetMaxFramesInFlight();
            pipelineInfo.m_physicalDevice = mp_VulkanManager->GetPhysicalDevice();
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

    Init();
}

void Loops::EngineManager::Init()
{
    for (auto& onStartFunc : m_appCallbacks.m_Start)
    {
        onStartFunc(mp_SceneManager->m_world);
    }
}

void Loops::EngineManager::DeInit()
{
    if (mp_VulkanManager->AreTheQueuesIdle())
    {
        for (auto& onExit : m_appCallbacks.m_Exit)
        {
            onExit();
        }

        ImguiEditor::DeInit();
        if (mp_ImguiSystem)
        {
            mp_ImguiSystem.reset();
            mp_ImguiSystem = nullptr;
        }

        if (mp_WireframePipeline)
        {
            mp_WireframePipeline.reset();
            mp_WireframePipeline = nullptr;
        }

        if (mp_BvhRenderPipeline)
        {
            mp_BvhRenderPipeline.reset();
            mp_BvhRenderPipeline = nullptr;
        }

        mp_SceneManager->DeInitialise();

        /*if (mp_ImguiUtil)
        {
            mp_ImguiUtil->Cleanup();
            mp_ImguiUtil.reset();
            mp_ImguiUtil = nullptr;
        }*/

        if (mp_InputManager)
        {
            mp_InputManager->DeInit();
            mp_InputManager.reset();
        }

        TextureManager::DeInit();

        Loops::Memory::MemoryManager::GetInstance()->DeInitVMA();

        mp_VulkanManager->DeInit();
        mp_WindowManagerObj->DeInit();

        mp_SceneManager.reset();
        mp_SceneManager = nullptr;

        mp_materialManager.reset();
        mp_materialManager = nullptr;
    }

    //if (mp_MemoryManager)
    //{
    //    mp_MemoryManager.reset();
    //    mp_MemoryManager = nullptr;
    //}

    Events::EventBus::GetInstance()->DeInit();
    delete Events::EventBus::GetInstance();

    Memory::MemoryManager::DeInit();

    if (mp_Timer)
    {
        mp_Timer.reset();
        mp_Timer = nullptr;
    }
}

//const Loops::ImguiUtil& Loops::EngineManager::GetImguiUtil() const
//{
//    return *mp_ImguiUtil.get();
//}

const Loops::SceneManager& Loops::EngineManager::GetSceneManager() const
{
    return *mp_SceneManager;
}

const Loops::VulkanManager& Loops::EngineManager::GetVulkanManager() const
{
    return *mp_VulkanManager;
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
    while (mp_WindowManagerObj->Update())
    {
        mp_Timer->StartFrame();

        auto currentFrameInFlight = mp_VulkanManager->GetFrameInFlightIndex();

        //mp_ImguiUtil->NewFrame(currentFrameInFlight);

        if (!m_isGameplayPaused)
        {
            for (auto& onUpdate : m_appCallbacks.m_Update)
            {
                onUpdate(mp_Timer->GetDeltaTime());
            }

            mp_SceneManager->Update(currentFrameInFlight);
            m_boundsManager.Update(currentFrameInFlight, mp_SceneManager->m_world);
        }
        mp_SceneManager->Prepare(currentFrameInFlight);

        switch (m_activePipeline)
        {
        case Tasking::PipelineType::WIREFRAME:
            mp_WireframePipeline->Update(currentFrameInFlight, mp_SceneManager, m_boundsManager, mp_VulkanManager, mp_ImguiSystem);
            break;

        case Tasking::PipelineType::BVH_RENDER:
            //mp_BvhRenderPipeline->Update(currentFrameInFlight, mp_SceneManager, m_boundsManager, mp_VulkanManager, mp_ImguiUtil);
            mp_BvhRenderPipeline->Update(currentFrameInFlight, mp_SceneManager, m_boundsManager, mp_VulkanManager, mp_ImguiSystem);
            break;

        default:
            break;
        }
        mp_Timer->EndFrame();
    }
}
