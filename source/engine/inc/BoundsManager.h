#ifndef BOUNDS_MANAGER_H
#define BOUNDS_MANAGER_H

#include <glm/glm.hpp>
#include "Defines.h"
#include <unordered_map>
#include <flecs.h>
#include <variant>
#include "memory/StackPoolAllocator.h"

namespace Loops
{

    struct MortonInfo
    {
        uint32_t m_boundId;
        uint32_t m_mortonCode;
    };

    struct LBVHTreelet
    {
        uint32_t m_startIndex, m_numPrimitives;
        Loops::BVHNode* m_bhvNode;
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
        //Bounds m_primitiveBounds[NUM_PRIMITIVE_BOUNDS];
        Bounds* m_primitiveBounds;
        uint32_t m_primitiveBoundCount = 0;

        // boundIndex to BoundInfo
        std::unordered_map<uint32_t, BoundInfo> m_primitiveBoundInfo;
        // meant to store the world space bounds, this will get re-arranged
        //Bounds m_primitiveWorldBounds[NUM_PRIMITIVE_BOUNDS];
        Bounds* m_primitiveWorldBounds;


        // Used while building the bvh
        uint32_t m_primitiveBoundIndicies[NUM_PRIMITIVE_BOUNDS];
        uint32_t m_primitiveBoundIndiciesCount = 0;

        BVHNode m_bvhNodeArray[NUM_BVH_NODES];
        mutable Bounds m_bvhNodeBoundArray[NUM_BVH_NODES];
        uint32_t m_bvhNodeCount = 0;

        // The tree node bounds will get stored in BVHBuildNode but the bound object should be fetched from a bound bound array
        Bounds m_sceneBound{};
        BVHNode* m_bvhRootNode = nullptr;

        mutable SplitMethod m_activeSplitMethod = SplitMethod::EQUAL_COUNT;
        mutable BvhCreationMethod m_activeCreationMethod = BvhCreationMethod::RECURSIVE;

        void CalculateSceneBound();
        void UpdatePrimtiveBoundInWorldSpace(const flecs::world& world);

        Memory::StackPoolAllocator* m_upperbvhStackAllocator = nullptr;
        Loops::BVHNode* BuildUpperBVH(std::vector<Loops::BVHNode*>& treeletArray, uint32_t start, uint32_t end, uint32_t* totalNodes);
        BVHNode* GetBvhNode(uint32_t numNodes = 1);

    public:
        BoundsManager();
        ~BoundsManager();
        void AddBound(const glm::vec3& min, const glm::vec3& max, uint32_t m_submeshId, uint32_t m_entityId);
        void Update(uint32_t currentFrameInFlight, const flecs::world& world);

        inline std::tuple< const Loops::Bounds*, uint32_t> GetPrimitiveBounds() const;
        inline std::tuple< const Loops::Bounds*, uint32_t> GetBvhNodeBounds() const;
        inline const std::unordered_map<uint32_t, BoundInfo>& GetPrimtiveBoundInfo() const;
        inline const BVHNode* GetRootNode() const;

        BVHNode* BuildBVHRecursive(Bounds* primitiveBoundsArray, uint32_t numPrimitiveBounds, uint32_t start, uint32_t end, uint32_t* totalNodes,
            uint32_t* orderedBoundsArrayIndiciesArray, uint32_t& orderedBoundIndidiciesCount, const SplitMethod& splitMethod);

        BVHNode* BuildHLBVH(Bounds* primitiveBoundsArray, uint32_t numPrimitiveBounds, uint32_t* totalNodes, 
            uint32_t* orderedBoundsArrayIndiciesArray, uint32_t& orderedBoundIndidiciesCount);

        void SetSplitType(const SplitMethod& splitMethod);
        void SetCreationMethod(const BvhCreationMethod& creationMethod);
        inline const SplitMethod& GetSplitType() const;
        inline const BvhCreationMethod& GetCreationMethod() const;
    };


    inline const Loops::SplitMethod& Loops::BoundsManager::GetSplitType() const
    {
        return m_activeSplitMethod;
    }

    inline const Loops::BvhCreationMethod& Loops::BoundsManager::GetCreationMethod() const
    {
        return m_activeCreationMethod;
    }

    inline const Loops::BVHNode* Loops::BoundsManager::GetRootNode() const
    {
        return m_bvhRootNode;
    }

    inline std::tuple< const Loops::Bounds*, uint32_t> Loops::BoundsManager::GetPrimitiveBounds() const
    {
        return { m_primitiveWorldBounds, m_primitiveBoundCount };
    }

    inline std::tuple< const Loops::Bounds*, uint32_t> Loops::BoundsManager::GetBvhNodeBounds() const
    {
        uint32_t i = 0;
        for (auto& node : m_bvhNodeArray)
        {
            m_bvhNodeBoundArray[i++] = node.m_bounds;
        }
        return { m_bvhNodeBoundArray, m_bvhNodeCount };
    }

    inline const std::unordered_map<uint32_t, Loops::BoundInfo>& Loops::BoundsManager::GetPrimtiveBoundInfo() const
    {
        return m_primitiveBoundInfo;
    }

}

#endif