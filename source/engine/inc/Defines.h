#ifndef DEFINES_H
#define DEFINES_H

#include <glm/glm.hpp>
#include <stdint.h>

namespace Loops
{
    // For now change this manually based on the model being loaded
    const uint32_t MAX_ENTITIES = 1000;
    const uint32_t MAX_MESH_VIEWS_PER_MESH = 10;

    constexpr uint32_t MAX_TREE_NODES = 50;
    constexpr uint32_t MAX_BOUNDS = MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH + MAX_TREE_NODES;

    struct alignas(4) Bounds
    {
        //glm::vec3 m_center{ 0.0f }; // CALCULATE CENTER
        glm::vec3 m_min{ 0.0f };
        glm::vec3 m_max{ 0.0f };
        uint32_t m_boundIndex = 0;
    };

    struct BoundInfo
    {
        uint32_t m_entityId;
        uint32_t m_submeshId;
    };

    struct Dimension
    {
        uint32_t m_width = 100;
        uint32_t m_height = 100;
    };

    struct Vertex
    {
        glm::vec4 m_position;
        glm::vec4 m_tangent;
        glm::vec3 m_normal;
        glm::vec2 m_uv;
    };
}

#endif