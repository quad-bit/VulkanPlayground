#ifndef IMGUI_EDITOR_H
#define IMGUI_EDITOR_H

#include "imgui.h"
#include "ImguiUtil.h"
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
        const ImguiUtil* m_utilObj;
        SceneManager* m_sceneManager;
        BoundsManager* m_boundsManager;
        int m_selectedNodeIndex = 0;

        static ImguiEditor* s_instancePtr;
        static std::mutex s_mtx;

        ImguiEditor() {}

        void DeInitPrivate();
    public:
        //ImguiEditor(const ImguiUtil& utilObj, const SceneManager& sceneManager, BoundsManager& boundsManager);

        static ImguiEditor* GetInstance();
        void Init(const ImguiUtil* utilObj, SceneManager* sceneManager, BoundsManager* boundsManager);
        static void DeInit();
        void AddPersistentCalls(const std::function<void()>& func);
    };
}

#endif // !IMGUI_EDITOR_H
