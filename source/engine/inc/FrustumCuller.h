#ifndef FRUSTUM_CULLER_H
#define FRUSTUM_CULLER_H

#include "math/Frustum.h"
#include "RenderData.h"
#include "Defines.h"
#include "Camera.h"

namespace Loops
{
    class FrustumCuller
    {
    private:
        Math::Frustum m_frustum;

        Math::CoverageStatus GetNodeCoverageWithAngle(const Bounds& bounds, const glm::vec3& nearPlaneCenter, const glm::vec3& farPlaneCenter, const glm::vec3& camPosition, const glm::vec3& camPosToFarCenter,
            const glm::vec3& camPosToNearCenter, float zFar, float zNear, float fov);
        Math::CoverageStatus GetNodeCoverageWithFrustum(const Bounds& bounds, const Math::Frustum& frustum, const glm::vec3& nearPlaneCenter, const glm::vec3& farPlaneCenter, const glm::vec3& camPosition);
        Math::CoverageStatus GetNodeCoverageWithProjection(const Bounds& bounds, const glm::vec3& nearPlaneCenter, const glm::vec3& farPlaneCenter, const glm::vec3& camPosition, const glm::vec3& camPosToFarCenter,
            const glm::vec3& camPosToNearCenter, float zFar, float zNear, float fov);

        void Cull(const Loops::BVHNode* node, const Math::Frustum& frustum, Math::CoverageStatus parentCoverage,
            const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc, const glm::vec3& nearPlaneCenter,
            const glm::vec3& farPlaneCenter, const glm::vec3& camPositionWS);

        void Cull(const Loops::BVHNode* node, const Math::Frustum& frustum, Math::CoverageStatus parentCoverage, const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc,
            const glm::vec3& nearPlaneCenter, const glm::vec3& farPlaneCenter, const glm::vec3& camPosition, const glm::vec3& camPosToFarCenter,
            const glm::vec3& camPosToNearCenter, float zFar, float zNear, float fov);

        void AddEntireBVHNode(const Loops::BVHNode* node, const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc);
    public:
        void PerformFrustumCulling(const Loops::BVHNode* rootNode, const glm::mat4& viewMat, const glm::mat4& projectionMat, const glm::mat4& modelMat,
            const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc);

        void PerformFrustumCulling(const Loops::BVHNode* rootNode, const Loops::Camera& camera, const glm::mat4& modelMat,
            const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc);
        FrustumCuller()
        {

        }
    };
}
#endif // !FRUSTUM_CULLER_H
