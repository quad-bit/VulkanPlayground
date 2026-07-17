#ifndef ENGINE_MANAGER_H
#define ENGINE_MANAGER_H

#include "SceneManager.h"
#include "BoundsManager.h"
#include "VulkanManager.h"
#include "WindowManager.h"
#include "memory/MemoryManager.h"
#include "input/InputManager.h"
#include "MaterialManager.h"

#include "ImguiUtil.h"
#include "Utils.h"
#include "pipelines/WireframePipeline.h"
#include "pipelines/BvhRenderPipeline.h"
#include "Timer.h"
#include "imgui/ImguiEditor.h"
#include "imgui/ImguiSystem.h"

#include <glm/glm.hpp>
#include <memory>

namespace Loops
{
    struct EngineInfo
    {
        Dimension m_screenSize{ 1280, 720 }, m_designSize{ 1280, 720 };
        std::vector<Loops::ModelLoadInfo> m_gltfInfos;
        std::vector<Tasking::PipelineType> m_pipelines;
        bool m_enableEditor = true;
        bool m_enableFullScreen = false;
    };

    struct AppCallbacks
    {
        std::vector < std::function<void(flecs::world&)>> m_Start;
        std::vector < std::function<void(const double&)>> m_Update;
        std::vector < std::function<void()>> mp_reRender;
        std::vector < std::function<void()>> m_Exit;
    };


    class EngineManager
    {
    private:
        std::unique_ptr<Loops::SceneManager> mp_SceneManager;
        std::unique_ptr<Loops::BoundsManager> mp_BoundsManager;
        std::unique_ptr<WindowManager> mp_WindowManagerObj;
        std::unique_ptr<VulkanManager> mp_VulkanManager;
        //std::unique_ptr<Loops::ImguiUtil > mp_ImguiUtil;
        //std::unique_ptr<Loops::ImguiEditor> mp_Editor;
        std::unique_ptr<Loops::ImguiSystem> mp_ImguiSystem;
        std::unique_ptr<Loops::MaterialManager> mp_materialManager;

        //std::unique_ptr<Loops::Memory::MemoryManager> mp_MemoryManager;
        std::unique_ptr<Loops::IO::InputManager> mp_InputManager;
        BoundsManager m_boundsManager;

        uint32_t m_maxFramesInFlight;
        std::unique_ptr<Timer> mp_Timer;

        bool m_isGameplayPaused = false;

        std::unique_ptr<Tasking::WireframePipeline> mp_WireframePipeline = nullptr;
        std::unique_ptr<Tasking::BvhRenderPipeline> mp_BvhRenderPipeline = nullptr;
        Tasking::PipelineType m_activePipeline;
        AppCallbacks m_appCallbacks;

        void Init();
    public:

        EngineManager(const EngineInfo& info, const AppCallbacks& callbacks = {});

        void DeInit();
        void Loop();

        //const ImguiUtil& GetImguiUtil() const;
        const SceneManager& GetSceneManager() const;
        const VulkanManager& GetVulkanManager() const;
        uint32_t GetMaxFrameInFlights() const;
        void ToggleGamePlayPauseState();
    };
}

#endif // !ENGINE_MANAGER_H
