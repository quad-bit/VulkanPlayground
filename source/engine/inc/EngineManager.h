#ifndef ENGINE_MANAGER_H
#define ENGINE_MANAGER_H

#include "SceneManager.h"
#include "BoundsManager.h"
#include "VulkanManager.h"
#include "WindowManager.h"
#include "memory/MemoryManager.h"
#include "input/InputManager.h"

#include "ImguiUtil.h"
#include "Utils.h"
#include "pipelines/WireframePipeline.h"
#include "Timer.h"
#include "ImguiEditor.h"

#include <glm/glm.hpp>
#include <memory>

namespace Loops
{
    struct EngineInfo
    {
        Dimension m_screenSize{ 1280, 720 }, m_designSize{ 1280, 720 };
        std::vector<Loops::ModelLoadInfo> m_gltfInfos;
        bool m_enableEditor = true;
    };

    struct AppCallbacks
    {
        std::vector < std::function<void(flecs::world&)>> m_Start;
        std::vector < std::function<void(const double&)>> m_Update;
        std::vector < std::function<void()>> m_PreRender;
        std::vector < std::function<void()>> m_Exit;
    };


    class EngineManager
    {
        private:
            std::unique_ptr<Loops::SceneManager> m_pSceneManager;
            std::unique_ptr<Loops::BoundsManager> m_pBoundsManager;
            std::unique_ptr<WindowManager> m_pWindowManagerObj;
            std::unique_ptr<VulkanManager> m_pVulkanManager;
            std::unique_ptr<Loops::ImguiUtil > m_pImguiUtil;
            std::unique_ptr<Loops::ImguiEditor> m_pEditor;
            std::unique_ptr<Loops::Memory::MemoryManager> m_pMemoryManager;
            std::unique_ptr<Loops::IO::InputManager> m_pInputManager;

            uint32_t m_maxFramesInFlight;
            std::unique_ptr<Timer> m_pTimer;

            std::unique_ptr<Tasking::WireframePipeline> m_pWireframePipeline;
            AppCallbacks m_appCallbacks;

            void Init();

        public:
            EngineManager(const EngineInfo& info, const AppCallbacks& callbacks = {});

            void DeInit();
            void Loop();

            const ImguiUtil& GetImguiUtil() const;
            const SceneManager& GetSceneManager() const;
            const VulkanManager& GetVulkanManager() const;
            uint32_t GetMaxFrameInFlights() const;

    };
}

#endif // !ENGINE_MANAGER_H
