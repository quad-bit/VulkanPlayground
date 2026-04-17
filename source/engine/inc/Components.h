#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Utils.h"
#include "Defines.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace Common
{
    // ============= TRANSFORM ===============
    struct Transform
    {
        glm::vec3 m_position{ 0.0f };
        glm::vec3 m_scale{ 1.0 };
        glm::vec3 m_eulerAngles{ 0.0 };
        glm::mat4x4 m_modelMat = glm::mat4(1.0);
        glm::mat4x4 m_modelMatGlobal = glm::mat4(1.0);
    };
    // ============= TRANSFORM ===============

    // ============= MESH ===============
    struct Vertex
    {
        glm::vec3 m_position;
        glm::vec4 m_tangent;
        glm::vec3 m_normal;
        glm::vec2 m_uv;
    };

    // one or more per object with a mesh (more in case of submesh)
    struct alignas(4) MeshView
    {
        uint32_t m_firstIndex, m_indexCount;
        uint32_t m_viewIndex;
    };

    // Multiple submeshes or primitives in one
    struct Mesh
    {
        MeshView m_meshViews[MAX_MESH_VIEWS_PER_MESH];
        uint16_t m_meshViewCount = 0;
        // ids of Vulkan wrappers
        uint16_t m_vertexBufferIndex, m_indexBufferIndex;
    };
    // ============= MESH ===============

}

#endif