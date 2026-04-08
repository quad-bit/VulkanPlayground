#pragma once

#include <vector>
#include "Defines.h"
#include "Components.h"

namespace Common
{
    //struct alignas(4) Drawable // REMOVE CONST CHAR* AND MAKE IT ALIGNED
    struct Drawable
    {
        uint32_t m_matIndex = 0;
        uint32_t m_viewStartIndex = 0, m_numOfViews = 0;
        uint32_t m_vertexBufferId;
        uint32_t m_indexBufferId; 

        const char* m_name;
    };

    struct RenderData
    {
        Common::MeshView m_meshViews[MAX_MESH_VIEWS_PER_MESH * MAX_ENTITIES];
        glm::mat4 m_modelMats[MAX_ENTITIES];
        Common::Drawable m_drawables[MAX_ENTITIES];
        uint32_t m_drawableCount = 0;
        uint32_t m_viewCount = 0;
    };
}