#pragma once
#include "Utils.h"

namespace Common
{
    struct Vertex
    {
        glm::vec4 m_position;
        glm::vec4 m_tangent;
        glm::vec3 m_normal;
        glm::vec2 m_uv;
    };

    // one or more per object with a mesh (more in case of submesh)
    struct MeshView
    {
        uint32_t m_firstIndex, m_indexCount;
    };

    // Multiple object or submeshes in one
    struct Mesh
    {
        std::vector<MeshView> m_meshViews;
    };
}