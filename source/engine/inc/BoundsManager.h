#ifndef BOUNDS_MANAGER_H
#define BOUNDS_MANAGER_H

#include <glm/glm.hpp>
#include "Defines.h"
#include <unordered_map>
#include <flecs.h>
#include <variant>

namespace Loops
{
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
        std::variant<BVHContainerNode, BVHLeafNode> m_node;

        void InitLeaf(uint32_t startIndex, uint32_t numBounds, const Bounds& bound);
        void InitContainer(uint8_t axis, BVHNode* child0, BVHNode* child1);
    };

    enum class SplitMethod
    {
        EQUAL_COUNT,
        MID,
        SAH
    };

    enum class BvhCreationMethod
    {
        RECURSIVE,
        LINEAR,
    };

    class BoundsManager
    {
    private:
        // meant to store the original min and max ( which will be in local space )
        Bounds m_primitiveBounds[NUM_PRIMITIVE_BOUNDS];
        uint32_t m_primitiveBoundCount = 0;

        // boundIndex to BoundInfo
        std::unordered_map<uint32_t, BoundInfo> m_primitiveBoundInfo;
        // meant to store the world space bounds, this will get re-arranged
        Bounds m_primitiveWorldBounds[NUM_PRIMITIVE_BOUNDS];

        // Used while building the bvh
        uint32_t m_primitiveBoundIndicies[NUM_PRIMITIVE_BOUNDS];
        uint32_t m_primitiveBoundIndiciesCount = 0;

        BVHNode m_bvhNodeArray[NUM_BVH_NODES];
        mutable Bounds m_bvhNodeBoundArray[NUM_BVH_NODES];
        uint32_t m_bvhNodeCount = 0;

        // The tree node bounds will get stored in BVHBuildNode but the bound object should be fetched from a bound bound array
        Bounds m_sceneBound;
        BVHNode* m_bvhRootNode;

        mutable SplitMethod m_activeSplitMethod = SplitMethod::EQUAL_COUNT;
        mutable BvhCreationMethod m_activeCreationMethod = BvhCreationMethod::RECURSIVE;

        void CalculateSceneBound();
        void UpdatePrimtiveBoundInWorldSpace(const flecs::world& world);

        BVHNode& GetBvhNode();


    public:
        BoundsManager();
        ~BoundsManager();
        void AddBound(const glm::vec3& min, const glm::vec3& max, uint32_t m_submeshId, uint32_t m_entityId);
        void Update(uint32_t currentFrameInFlight, const flecs::world& world);

        std::tuple< const Loops::Bounds*, uint32_t> GetPrimitiveBounds() const;
        std::tuple< const Loops::Bounds*, uint32_t> GetBvhNodeBounds() const;
        const std::unordered_map<uint32_t, BoundInfo>& GetPrimtiveBoundInfo() const;

        BVHNode* BuildBVHRecursive(Bounds* primitiveBoundsArray, uint32_t numPrimitiveBounds, uint32_t start, uint32_t end, uint32_t& totalNodes,
            uint32_t* orderedBoundsArrayIndiciesArray, uint32_t& orderedBoundIndidiciesCount, const SplitMethod& splitMethod);

        void SetSplitType(const SplitMethod& splitMethod);
        void SetCeationMethod(const BvhCreationMethod& creationMethod);
        const SplitMethod& GetSplitType() const;
        const BvhCreationMethod& GetCreationMethod() const;
    };
}

#endif