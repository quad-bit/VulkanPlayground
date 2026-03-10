#pragma once

#include "imgui.h"
#include "ImguiUtil.h"
#include "SceneManager.h"

namespace Common
{
    class ImguiEditor
    {
    private:
        const ImguiUtil& cm_utilObj;
        const SceneManager& cm_sceneManager;

        int m_selectedNodeIndex = 0;

    public:
        ImguiEditor(const ImguiUtil& utilObj, const SceneManager& sceneManager);

    };
}