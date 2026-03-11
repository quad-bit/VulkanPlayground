#pragma once

#include "Utils.h"

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
    // ============= MESH ===============

}