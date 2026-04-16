#ifndef BOUNDS_MANAGER_H
#define BOUNDS_MANAGER_H

#include <glm/glm.hpp>
#include "Defines.h"

namespace Common
{
    constexpr uint32_t MAX_BOUNDS = MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH;
    struct alignas(4) AABB
    {
        glm::vec3 m_center{ 0.0f };
        uint32_t m_boundIndex = 0;
        glm::vec3 m_min{0.0f}, m_max{ 0.0f };
    };
    
    constexpr uint32_t MAX_TREE_NODES = 50;
    
    struct BoundInfo
    {
        uint32_t m_boundIndex;
        uint32_t m_entityId;
        uint32_t m_submeshId;
        glm::mat4* pm_globalMat;
    };

    AABB Union(const AABB& b1, const AABB& b2);

    class BoundsManager
    {
    private:
        uint32_t m_primitiveBoundCount = 0;
        AABB m_bounds[MAX_BOUNDS];
        BoundInfo m_infos[MAX_BOUNDS];
        AABB m_sceneBound;

        uint32_t m_treeNodeBoundCount = 0;
        Common::AABB m_treeNodeBounds[Common::MAX_TREE_NODES];

        void CalculateSceneBound();

    public:
        void AddBound(const glm::vec3& min, const glm::vec3& max, glm::mat4* pm_globalMat, uint32_t m_submeshId, uint32_t m_entityId);
        void Update(uint32_t currentFrameInFlight);
    };
}

#endif