#ifndef DEFINES_H
#define DEFINES_H

#include <glm/glm.hpp>
#include <stdint.h>
#include <variant>

#define BVH_SCENE_VIEW_ENABLED

namespace Loops
{
    // For now change this manually based on the model being loaded
    const uint32_t MAX_ENTITIES = 1100;
    const uint32_t MAX_MESH_VIEWS_PER_MESH = 1;

    constexpr uint32_t NUM_PRIMITIVE_BOUNDS = MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH;
    constexpr uint32_t NUM_BVH_NODES = 2 * NUM_PRIMITIVE_BOUNDS - 1;
    constexpr uint32_t MAX_BOUNDS = NUM_PRIMITIVE_BOUNDS + NUM_BVH_NODES;
    constexpr uint32_t MAX_PRIMITIVES_PER_LEAF = 2;

    struct alignas(4) Bounds
    {
        //glm::vec3 m_center{ 0.0f }; // CALCULATE CENTER
        glm::vec3 m_max{ std::numeric_limits<float>::lowest() };
        glm::vec3 m_min{ std::numeric_limits<float>::max() };
        uint32_t m_boundIndex = 0;
    };

    struct BoundInfo
    {
        uint32_t m_entityId;
        uint32_t m_submeshId;
    };

    // ============ BVH
    class BVHNode;
    struct BVHContainerNode
    {
        uint8_t m_splitAxis; // only 3 dimensions 0 : x, 1 : y, 2 : z
        BVHNode* m_child0 = nullptr;
        BVHNode* m_child1 = nullptr;
    };

    struct BVHLeafNode
    {
        uint32_t m_startIndex = 0;
        uint32_t m_numBounds = 0;
    };

    struct BVHNode
    {
        Bounds m_bounds;
        uint32_t m_index = 0;
        std::variant<BVHContainerNode, BVHLeafNode> m_node;
        void InitLeaf(uint32_t startIndex, uint32_t numBounds, const Bounds& bound);
        void InitContainer(uint8_t axis, BVHNode* child0, BVHNode* child1);
    };
    // ============ BVH

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