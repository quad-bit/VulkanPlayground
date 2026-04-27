#ifndef ENGINE_MANAGER_H
#define ENGINE_MANAGER_H

#include "SceneManager.h"
#include "BoundsManager.h"
#include "VulkanManager.h"
#include "WindowManager.h"
#include "memory/MemoryManager.h"

#include "ImguiUtil.h"
#include "Utils.h"
#include "pipelines/WireframePipeline.h"
#include "Timer.h"
#include "ImguiEditor.h"

#include <glm/glm.hpp>
#include <memory>

namespace Common
{
    struct EngineInfo
    {
        Dimension m_screenSize{ 1280, 720 }, m_designSize{ 1280, 720 };
        std::vector<Common::ModelLoadInfo> m_gltfInfos;
        bool m_enableEditor = true;
    };


    class EngineManager
    {
        private:
            std::unique_ptr<Common::SceneManager> m_pSceneManager;
            std::unique_ptr<Common::BoundsManager> m_pBoundsManager;
            std::unique_ptr<WindowManager> m_pWindowManagerObj;
            std::unique_ptr<VulkanManager> m_pVulkanManager;
            std::unique_ptr<Common::ImguiUtil > m_pImguiUtil;
            std::unique_ptr<Common::ImguiEditor> m_pEditor;
            std::unique_ptr<Common::Memory::MemoryManager> m_pMemoryManager;

            uint32_t m_maxFramesInFlight;
            std::unique_ptr<Timer> m_pTimer;

            std::unique_ptr<Tasking::WireframePipeline> m_pWireframePipeline;

        public:
            EngineManager(const EngineInfo& info);
            void Init();
            void DeInit();
            void Loop();

            const ImguiUtil& GetImguiUtil() const;
            const SceneManager& GetSceneManager() const;
            const VulkanManager& GetVulkanManager() const;
            uint32_t GetMaxFrameInFlights() const;

    };
}

#endif // !ENGINE_MANAGER_H
