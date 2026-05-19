#ifndef RENDER_DATA_H
#define RENDER_DATA_H

#include <vector>
#include "Defines.h"
#include "Components.h"

namespace Loops
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

    // Camera data
    struct CameraData
    {
        glm::mat4 m_viewMat;
        glm::mat4 m_projectionMat;
        glm::vec3 m_cameraPos;
    };

    struct RenderData
    {
        Loops::MeshView m_meshViews[MAX_MESH_VIEWS_PER_MESH * MAX_ENTITIES];
        glm::mat4 m_modelMats[MAX_ENTITIES];
        Loops::Drawable m_drawables[MAX_ENTITIES];
        uint32_t m_drawableCount = 0;
        uint32_t m_viewCount = 0;
        CameraData m_cameraData;
    };
}

#endif // !RENDER_DATA_H
