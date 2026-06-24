#ifndef IMGUI_EDITOR_H
#define IMGUI_EDITOR_H

#include "imgui.h"
#include "ImguiUtil.h"
#include "imgui/ImguiSystem.h"
#include "SceneManager.h"
#include "BoundsManager.h"
#include <flecs.h>
#include <mutex>
#include <functional>

namespace Loops
{
    class ImguiEditor
    {
    private:
        //ImguiUtil* m_utilObj;
        ImguiSystem* m_utilObj = nullptr;
        SceneManager* m_sceneManager = nullptr;
        BoundsManager* m_boundsManager = nullptr;
        int m_selectedNodeIndex = 0;

        static ImguiEditor* s_instancePtr;
        static std::mutex s_mtx;

        ImguiEditor() {}

        void DeInitPrivate();
    public:
        //ImguiEditor(const ImguiUtil& utilObj, const SceneManager& sceneManager, BoundsManager& boundsManager);

        static ImguiEditor* GetInstance();
        //void Init(ImguiUtil* utilObj, SceneManager* sceneManager, BoundsManager* boundsManager);
        void Init(ImguiSystem* utilObj, SceneManager* sceneManager, BoundsManager* boundsManager);
        static void DeInit();
        void AddPersistentCalls(const std::function<void()>& func);
        void AddPersistentCalls(const std::function<void(uint32_t)>& func);
    };
}

#endif // !IMGUI_EDITOR_H
