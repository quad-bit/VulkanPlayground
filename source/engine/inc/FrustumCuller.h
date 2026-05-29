#ifndef FRUSTUM_CULLER_H
#define FRUSTUM_CULLER_H

#include "math/Frustum.h"
#include "RenderData.h"
#include "Defines.h"

namespace Loops
{
    class FrustumCuller
    {
    private:
        Math::Frustum m_frustum;
        void Cull(const Loops::BVHNode* node, const Math::Frustum& frustum, Math::CoverageStatus parentCoverage,
            const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc);
        void AddEntireBVHNode(const Loops::BVHNode* node, const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc);
    public:
        void PerformFrustumCulling(const Loops::BVHNode* rootNode, const glm::mat4& viewMat, const glm::mat4& projectionMat,
            const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc);
    };
}
#endif // !FRUSTUM_CULLER_H
