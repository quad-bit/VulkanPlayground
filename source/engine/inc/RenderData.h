#ifndef RENDER_DATA_H
#define RENDER_DATA_H

#include <vector>
#include "Defines.h"
#include "Components.h"

namespace Loops
{
    // Each Drawable will contain views with same material
    //struct alignas(4) Drawable // REMOVE CONST CHAR* AND MAKE IT ALIGNED
    struct Drawable
    {
        uint32_t m_matrixIndex = 0;
        uint32_t m_materialIndex = 0;
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
        glm::vec4 m_cameraPos;
    };

    struct alignas(16) LightUniform
    {
        glm::mat4 m_lightMatrix;
        glm::vec4 m_posOrDir; // w=0 for directional, w=1 for point lights
        glm::vec4 m_ambient;
        glm::vec4 m_specular;
        glm::vec4 m_diffuse;
        int m_shadowMapIndex;
    };

    struct RenderData
    {
        Loops::MeshView m_meshViews[MAX_MESH_VIEWS_PER_MESH * MAX_ENTITIES];
        glm::mat4 m_modelMats[MAX_ENTITIES];
        Loops::Drawable m_drawables[MAX_MESH_VIEWS_PER_MESH * MAX_ENTITIES];
        uint32_t m_drawableCount = 0;
        // drawable indicies per material type
        std::unordered_map<Loops::EFFECT_TYPE, std::unordered_map<Loops::TECHNIQUE_TYPE, std::vector<uint32_t>>> m_drawablesPerMaterial;
        uint32_t m_viewCount = 0;
        CameraData m_cameraData;
    };
}

#endif // !RENDER_DATA_H
