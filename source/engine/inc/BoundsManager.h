#ifndef BOUNDS_MANAGER_H
#define BOUNDS_MANAGER_H

#include <glm/glm.hpp>
#include "Defines.h"
#include <unordered_map>
#include <flecs.h>

namespace Loops
{

    Bounds Union(const Bounds& b1, const Bounds& b2);

    class BoundsManager
    {
    private:
        // meant to store the original min and max ( which will be in local space )
        Bounds m_primitiveBounds[MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH];
        uint32_t m_primitiveBoundCount = 0;

        // boundIndex to BoundInfo
        std::unordered_map<uint32_t, BoundInfo> m_primitiveBoundInfo;
        // meant to store the world space bounds, this will get re-arranged
        Bounds m_primitiveWorldBounds[MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH];

        // The tree node bounds will get stored in BVHBuildNode but the bound object should be fetched from a bound bound array
        Bounds m_sceneBound;

        void CalculateSceneBound();
        void UpdatePrimtiveBoundInWorldSpace(const flecs::world& world);
    public:
        BoundsManager();
        ~BoundsManager();
        void AddBound(const glm::vec3& min, const glm::vec3& max, uint32_t m_submeshId, uint32_t m_entityId);
        void Update(uint32_t currentFrameInFlight, const flecs::world& world);

        std::tuple< const Loops::Bounds*, uint32_t> GetPrimitiveBounds() const;
        const std::unordered_map<uint32_t, BoundInfo>& GetPrimtiveBoundInfo() const;
    };
}

#endif