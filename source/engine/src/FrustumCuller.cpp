#include "FrustumCuller.h"
#include "Assertion.h"

// simd glm
#include <glm/gtx/component_wise.hpp> // for compMin / compMax
#include <glm/gtx/vector_angle.hpp> // For glm::angle
#include <glm/gtx/projection.hpp> // For glm::proj

#define COVERAGE_ANGLE 0;
#define COVERAGE_FRUSTUM 1;
#define COVERAGE_PROJECTION 2;

#define COVERAGE_CHECK_TYPE COVERAGE_FRUSTUM;

Loops::Math::CoverageStatus Loops::FrustumCuller::GetNodeCoverageWithAngle(const Bounds& bounds, const glm::vec3& nearPlaneCenter,
    const glm::vec3& farPlaneCenter, const glm::vec3& camPosition, const glm::vec3& camPosToFarCenter,
    const glm::vec3& camPosToNearCenter, float zFar, float zNear, float fov)
{
    auto GetCoverage = [&](const glm::vec3& point) -> Math::CoverageStatus
        {
            const glm::vec3 camToPoint = point - camPosition;

            glm::vec3 v1 = glm::normalize(camToPoint);
            glm::vec3 v2 = glm::normalize(camPosToFarCenter);

            // Calculate angle in radians
            float angleDeg = glm::degrees(glm::angle(v1, v2));

            // hypotnuse 
            float extremeCornerLength = zFar/cos(glm::radians(fov / 2.0f)); //or length(camPosToFarCenter)
            float pointDistance = glm::length(point - camPosition);

            // ignoring the near distance
            if (angleDeg <= fov && pointDistance <= extremeCornerLength)
                return Math::CoverageStatus::INCLUDED;
            else
                return Math::CoverageStatus::EXCLUDED;
        };

    const Math::CoverageStatus minCoverage = GetCoverage(bounds.m_min);
    const Math::CoverageStatus maxCoverage = GetCoverage(bounds.m_max);

    Math::CoverageStatus totalCoverage{Math::CoverageStatus::EXCLUDED};
    if (minCoverage != Math::CoverageStatus::EXCLUDED && maxCoverage != Math::CoverageStatus::EXCLUDED)
        totalCoverage = Math::CoverageStatus::INCLUDED;

    if ((minCoverage == Math::CoverageStatus::EXCLUDED && maxCoverage == Math::CoverageStatus::INCLUDED)||
        (minCoverage == Math::CoverageStatus::INCLUDED && maxCoverage == Math::CoverageStatus::EXCLUDED))
            totalCoverage = Math::CoverageStatus::PARTIAL;

    if ((nearPlaneCenter.x > bounds.m_min.x && nearPlaneCenter.y > bounds.m_min.y && nearPlaneCenter.z > bounds.m_min.z) &&
        (nearPlaneCenter.x < bounds.m_max.x && nearPlaneCenter.y < bounds.m_max.y && nearPlaneCenter.z < bounds.m_max.z))
        totalCoverage = Math::CoverageStatus::PARTIAL;

    return totalCoverage;
}

Loops::Math::CoverageStatus Loops::FrustumCuller::GetNodeCoverageWithFrustum(const Bounds& bounds, const Math::Frustum& frustum,
    const glm::vec3& nearPlaneCenter, const glm::vec3& farPlaneCenter, const glm::vec3& camPosition)
{
    auto coverage = frustum.IsBoxVisible(bounds.m_min, bounds.m_max);
    // slab's method
    if (coverage == Math::CoverageStatus::EXCLUDED)
    {
        glm::vec3 d = farPlaneCenter - nearPlaneCenter;
        glm::vec3 invD = 1.0f / d;
        glm::vec3 t0 = (bounds.m_min - nearPlaneCenter) * invD;
        glm::vec3 t1 = (bounds.m_max - nearPlaneCenter) * invD;
        glm::vec3 tminVec = glm::min(t0, t1);
        glm::vec3 tmaxVec = glm::max(t0, t1);

        float tmin = glm::compMax(tminVec);
        float tmax = glm::compMin(tmaxVec);

        if ((tmax >= tmin) && (tmax >= 0.0f) && (tmin <= 1.0f))
            coverage = Math::CoverageStatus::PARTIAL;
    }
    return coverage;
}

Loops::Math::CoverageStatus Loops::FrustumCuller::GetNodeCoverageWithProjection(const Bounds& bounds, const glm::vec3& nearPlaneCenter,
    const glm::vec3& farPlaneCenter, const glm::vec3& camPosition, const glm::vec3& camPosToFarCenter, const glm::vec3& camPosToNearCenter,
    float zFar, float zNear, float fov)
{
    const glm::vec3& max = bounds.m_max;
    const glm::vec3& min = bounds.m_min;

    // vector from cam position to min and max
    const glm::vec3 minVector = min - camPosition;
    const glm::vec3 maxVector = max - camPosition;

    // projection min vector on camFar vector
    const float projOnFrustumVerticalCenterLineMinVector = glm::dot(camPosToFarCenter, minVector) / glm::length(camPosToFarCenter);
    // projection max vector on camFar vector
    const float projOnFrustumVerticalCenterLineMaxVector = glm::dot(camPosToFarCenter, maxVector) / glm::length(camPosToFarCenter);

    bool isMinVerticallyIn = false, isMaxVerticallyIn = false;
    float farDistanceBias = 20.0f;
    if (projOnFrustumVerticalCenterLineMaxVector > 0)
    {
        if (projOnFrustumVerticalCenterLineMaxVector < zFar + farDistanceBias && projOnFrustumVerticalCenterLineMaxVector >= zNear)
            isMaxVerticallyIn = true;
    }

    if (projOnFrustumVerticalCenterLineMinVector > 0)
    {
        if (projOnFrustumVerticalCenterLineMinVector < zFar + farDistanceBias && projOnFrustumVerticalCenterLineMinVector >= zNear)
            isMinVerticallyIn = true;
    }

    if (isMaxVerticallyIn || isMinVerticallyIn)
    {    // check if they both are on same or oppotiste side camToFarVector
        float val = glm::dot(camPosToFarCenter, minVector) * glm::dot(camPosToFarCenter, maxVector);
        //{-1 * 1}, { 1 * 1, -1 * -1 }
        if (val >= 0)
        {
            float minAngle = glm::angle(glm::normalize(minVector), glm::normalize(camPosToFarCenter));
            float maxAngle = glm::angle(glm::normalize(maxVector), glm::normalize(camPosToFarCenter));

            float bias = 15.0f;
            if (minAngle > glm::radians(fov/2.0f + bias) && maxAngle > glm::radians(fov/2.0f + bias))
                return Math::CoverageStatus::EXCLUDED;
        }
        return Math::CoverageStatus::PARTIAL;
    }
    else
        return Math::CoverageStatus::EXCLUDED;

    //// NOT CEHCKING HORIZONTAL

    // tan(fov/2) = frustumHorizontalLength/projOnFrustumVerticalCenterLineMinVector;
    const float frustumHorizontalLengthMin = glm::tan(glm::radians(fov / 2.0f)) * projOnFrustumVerticalCenterLineMinVector;
    const float frustumHorizontalLengthMax = glm::tan(glm::radians(fov / 2.0f)) * projOnFrustumVerticalCenterLineMaxVector;

    // calculation position of point at frustumHorizontalLengthMin and frustumHorizontalLengthMax distance from cam position in the directiion
    // camPosToFarCenter, campos + unit vector * distance
    const glm::vec3 minPostionOnCenterLine = camPosition + glm::normalize(camPosToFarCenter) * projOnFrustumVerticalCenterLineMinVector;
    const glm::vec3 maxPostionOnCenterLine = camPosition + glm::normalize(camPosToFarCenter) * projOnFrustumVerticalCenterLineMaxVector;

    const float horizontalMaxDistance = glm::length(maxPostionOnCenterLine - max);
    const float horizontalMinDistance = glm::length(minPostionOnCenterLine - min);

    bool isMinHorizontalyIn = false, isMaxHorizontalyIn = false;
    if (horizontalMinDistance <= frustumHorizontalLengthMin)
    {
        isMinHorizontalyIn = true;
    }

    if (horizontalMaxDistance <= frustumHorizontalLengthMax)
    {
        isMaxHorizontalyIn = true;
    }

    if (isMaxVerticallyIn && isMinVerticallyIn && isMaxHorizontalyIn && isMinHorizontalyIn)
        return Math::CoverageStatus::INCLUDED;

    if (!isMaxVerticallyIn && !isMinVerticallyIn && !isMaxHorizontalyIn && !isMinHorizontalyIn)
    {
        /*if((nearPlaneCenter.x > min.x && nearPlaneCenter.y > min.y && nearPlaneCenter.z > min.z)&&
            (nearPlaneCenter.x < max.x && nearPlaneCenter.y < max.y && nearPlaneCenter.z < max.z))
            return Math::CoverageStatus::PARTIAL;*/
        return Math::CoverageStatus::EXCLUDED;
    }

    if ((isMaxVerticallyIn && isMaxHorizontalyIn) || (isMinVerticallyIn && isMinHorizontalyIn))
        return Math::CoverageStatus::PARTIAL;

    return Math::CoverageStatus::EXCLUDED;
}

void Loops::FrustumCuller::PerformFrustumCulling(const Loops::BVHNode* rootNode, const glm::mat4& viewMat, const glm::mat4& projectionMat,
    const glm::mat4& modelMat, const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc)
{
    m_frustum.Update(viewMat, projectionMat);

    // in clip space
    glm::vec4 nearPlanceCenter{ 0,0,0,1 }, farPlaneCenter{ 0,0,1,1 };
    auto ConvertToWSpace = [&viewMat, &projectionMat](const glm::vec4& point) -> glm::vec3
        {
            glm::vec4 p = glm::inverse(projectionMat) * point;
            p /= p.w;
            p = glm::inverse(viewMat) * p;
            return glm::vec3(p.x, p.y, p.z);
        };

    auto nearPlanceCenterWS = ConvertToWSpace(nearPlanceCenter);
    auto farPlanceCenterWS = ConvertToWSpace(farPlaneCenter);
    auto camPositionWS = glm::vec3(modelMat[3]);
    Cull(rootNode, m_frustum, Math::CoverageStatus::NUM_STATUS, onLeafNodeEntryFunc, nearPlanceCenterWS, farPlanceCenterWS, camPositionWS);
}

void Loops::FrustumCuller::PerformFrustumCulling(const Loops::BVHNode* rootNode, const Loops::Camera& camera, const glm::mat4& modelMat,
    const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc)
{
    auto& viewMat = camera.GetViewMatrix();
    auto& projectionMat = camera.GetProjectionMat();
    m_frustum.Update(viewMat, projectionMat);

    // in clip space
    glm::vec4 nearPlanceCenter{ 0,0,0,1 }, farPlaneCenter{ 0,0,1,1 };
    auto ConvertToWSpace = [&viewMat, &projectionMat](const glm::vec4& point) -> glm::vec3
        {
            glm::vec4 p = glm::inverse(projectionMat) * point;
            p /= p.w;
            p = glm::inverse(viewMat) * p;
            return glm::vec3(p.x, p.y, p.z);
        };

    auto nearPlanceCenterWS = ConvertToWSpace(nearPlanceCenter);
    auto farPlanceCenterWS = ConvertToWSpace(farPlaneCenter);
    auto camPositionWS = glm::vec3(modelMat[3]);
    glm::vec3 camPosToFarCenter = farPlanceCenterWS - camPositionWS;
    glm::vec3 camPosToNearCenter = nearPlanceCenterWS - camPositionWS;
    float farDistance = camera.GetFarZ();
    float nearDistance = camera.GetNearZ();
    float fov = camera.GetFov();

    Cull(rootNode, m_frustum, Math::CoverageStatus::NUM_STATUS, onLeafNodeEntryFunc, nearPlanceCenterWS, farPlanceCenterWS, camPositionWS,
        camPosToFarCenter, camPosToNearCenter, farDistance, nearDistance, fov);
}

void Loops::FrustumCuller::Cull(const Loops::BVHNode* node, const Math::Frustum& frustum, Loops::Math::CoverageStatus parentCoverage, const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc,
    const glm::vec3& nearPlaneCenter, const glm::vec3& farPlaneCenter, const glm::vec3& camPosition, const glm::vec3& camPosToFarCenter,
    const glm::vec3& camPosToNearCenter, float zFar, float zNear, float fov)
{

    Math::CoverageStatus nodeCoverage{ Math::CoverageStatus::NUM_STATUS };
    const Bounds nodeBound{ node->m_bounds };

    bool isNodeContainer{ false };

    if (std::holds_alternative<BVHContainerNode>(node->m_node))
        isNodeContainer = true;

    switch (parentCoverage)
    {
    case Math::CoverageStatus::NUM_STATUS:
    {
        // Only possible with root node
#if COVERAGE_CHECK_TYPE == COVERAGE_FRUSTUM
        nodeCoverage = GetNodeCoverageWithFrustum(nodeBound, m_frustum, nearPlaneCenter, farPlaneCenter, camPosition);
        if (!isNodeContainer && nodeCoverage == Math::CoverageStatus::PARTIAL)
            /*nodeCoverage = GetNodeCoverageWithAngle(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition,
                camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);*/
            nodeCoverage = GetNodeCoverageWithProjection(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition, camPosToFarCenter, camPosToNearCenter,
                zFar, zNear, fov);
#elif COVERAGE_CHECK_TYPE == COVERAGE_ANGLE
        nodeCoverage = GetNodeCoverageWithAngle(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition,
            camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
#elif COVERAGE_CHECK_TYPE == COVERAGE_PROJECTION
        nodeCoverage = GetNodeCoverageWithProjection(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition,
            camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
#endif
        if (nodeCoverage == Math::CoverageStatus::EXCLUDED)
            return;

        if (nodeCoverage == Math::CoverageStatus::INCLUDED)
            AddEntireBVHNode(node, onLeafNodeEntryFunc);
        else if (isNodeContainer)
        {
            Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition,
                camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
            Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition,
                camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
        }
        else
            AddEntireBVHNode(node, onLeafNodeEntryFunc);
        return;
    }
    break;

    case Math::CoverageStatus::INCLUDED:
    {
        if (isNodeContainer)
        {
            AddEntireBVHNode(node, onLeafNodeEntryFunc);
        }
        else
        {
            //AddLeafPrimitivesToRenderData(node, renderData);
            onLeafNodeEntryFunc(node);
        }
        return;
    }
    break;

    case Math::CoverageStatus::PARTIAL:
    {
#if COVERAGE_CHECK_TYPE == COVERAGE_FRUSTUM
        nodeCoverage = GetNodeCoverageWithFrustum(nodeBound, m_frustum, nearPlaneCenter, farPlaneCenter, camPosition);
        if (!isNodeContainer && nodeCoverage == Math::CoverageStatus::PARTIAL)
            /*nodeCoverage = GetNodeCoverageWithAngle(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition,
                camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);*/
            nodeCoverage = GetNodeCoverageWithProjection(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition, camPosToFarCenter, camPosToNearCenter,
                zFar, zNear, fov);
#elif COVERAGE_CHECK_TYPE == COVERAGE_ANGLE
        nodeCoverage = GetNodeCoverageWithAngle(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition,
            camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
#elif COVERAGE_CHECK_TYPE == COVERAGE_PROJECTION
        nodeCoverage = GetNodeCoverageWithProjection(nodeBound, nearPlaneCenter, farPlaneCenter, camPosition,
            camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
#endif
        if (nodeCoverage == Math::CoverageStatus::EXCLUDED)
            /*if ((nearPlaneCenter.x < nodeBound.m_max.x && nearPlaneCenter.y < nodeBound.m_max.y && nearPlaneCenter.z < nodeBound.m_max.z) ||
                (nearPlaneCenter.x > nodeBound.m_min.x && nearPlaneCenter.y > nodeBound.m_min.y && nearPlaneCenter.z > nodeBound.m_min.z))
            {
                nodeCoverage = Math::CoverageStatus::PARTIAL;
                Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
                Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
            }
            else*/
            return;

        if (isNodeContainer)
        {
            if (nodeCoverage == Math::CoverageStatus::INCLUDED)
                AddEntireBVHNode(node, onLeafNodeEntryFunc);
            else if (isNodeContainer)
            {
                Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition,
                    camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
                Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition,
                    camPosToFarCenter, camPosToNearCenter, zFar, zNear, fov);
            }
            else
                AddEntireBVHNode(node, onLeafNodeEntryFunc);
            return;
        }
        else
        {
            onLeafNodeEntryFunc(node);
        }
        return;
    }
    break;

    case Math::CoverageStatus::EXCLUDED:
        // check case where frustum is inside the bounding box 
        /*if ((nearPlaneCenter.x < nodeBound.m_max.x && nearPlaneCenter.y < nodeBound.m_max.y && nearPlaneCenter.z < nodeBound.m_max.z) ||
            (nearPlaneCenter.x > nodeBound.m_min.x && nearPlaneCenter.y > nodeBound.m_min.y && nearPlaneCenter.z > nodeBound.m_min.z))
        {
            nodeCoverage = Math::CoverageStatus::PARTIAL;
            Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
            Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
        }
        else*/
        return;
        break;
    default:
    {
        return;
    }
    break;
    }
}


void Loops::FrustumCuller::Cull(const Loops::BVHNode* node, const Math::Frustum& frustum, Math::CoverageStatus parentCoverage,
    const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc, const glm::vec3& nearPlaneCenter,
    const glm::vec3& farPlaneCenter, const glm::vec3& camPosition)
{
    Math::CoverageStatus nodeCoverage{ Math::CoverageStatus::NUM_STATUS };
    const Bounds nodeBound{ node->m_bounds };

    bool isNodeContainer{ false };

    if (std::holds_alternative<BVHContainerNode>(node->m_node))
        isNodeContainer = true;

    auto CheckBoundCoverage = [&nearPlaneCenter, &farPlaneCenter, &camPosition](const Bounds& bound, const Math::Frustum& frustum) -> Math::CoverageStatus
        {
            auto coverage = frustum.IsBoxVisible(bound.m_min, bound.m_max);

            // slab's method
            /*if (coverage == Math::CoverageStatus::EXCLUDED)
            {
                glm::vec3 d = farPlaneCenter - nearPlaneCenter;
                glm::vec3 invD = 1.0f / d;
                glm::vec3 t0 = (bound.m_min - nearPlaneCenter) * invD;
                glm::vec3 t1 = (bound.m_max - nearPlaneCenter) * invD;
                glm::vec3 tminVec = glm::min(t0, t1);
                glm::vec3 tmaxVec = glm::max(t0, t1);

                float tmin = glm::compMax(tminVec);
                float tmax = glm::compMin(tmaxVec);

                if ((tmax >= tmin) && (tmax >= 0.0f) && (tmin <= 1.0f))
                    coverage = Math::CoverageStatus::PARTIAL;
            }*/
            return coverage;
        };

    switch (parentCoverage)
    {
    case Math::CoverageStatus::NUM_STATUS:
    {
        // Only possible with root node
        nodeCoverage = CheckBoundCoverage(nodeBound, frustum);
        if (nodeCoverage == Math::CoverageStatus::EXCLUDED)
            return;

        if (nodeCoverage == Math::CoverageStatus::INCLUDED)
            AddEntireBVHNode(node, onLeafNodeEntryFunc);
        else if(isNodeContainer)
        {
            Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition);
            Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition);
        }
        else
            AddEntireBVHNode(node, onLeafNodeEntryFunc);
        return;
    }
    break;

    case Math::CoverageStatus::INCLUDED:
    {
        if (isNodeContainer)
        {
            AddEntireBVHNode(node, onLeafNodeEntryFunc);
        }
        else
        {
            //AddLeafPrimitivesToRenderData(node, renderData);
            onLeafNodeEntryFunc(node);
        }
        return;
    }
    break;

    case Math::CoverageStatus::PARTIAL:
    {
        nodeCoverage = CheckBoundCoverage(nodeBound, frustum);
        if (nodeCoverage == Math::CoverageStatus::EXCLUDED)
            /*if ((nearPlaneCenter.x < nodeBound.m_max.x && nearPlaneCenter.y < nodeBound.m_max.y && nearPlaneCenter.z < nodeBound.m_max.z) ||
                (nearPlaneCenter.x > nodeBound.m_min.x && nearPlaneCenter.y > nodeBound.m_min.y && nearPlaneCenter.z > nodeBound.m_min.z))
            {
                nodeCoverage = Math::CoverageStatus::PARTIAL;
                Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
                Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
            }
            else*/
                return;

        if (isNodeContainer)
        {
            if (nodeCoverage == Math::CoverageStatus::INCLUDED)
                AddEntireBVHNode(node, onLeafNodeEntryFunc);
            else if( isNodeContainer)
            {
                Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition);
                Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter, camPosition);
            }
            else
                AddEntireBVHNode(node, onLeafNodeEntryFunc);
            return;
        }
        else
        {
            //AddLeafPrimitivesToRenderData(node, renderData);
            onLeafNodeEntryFunc(node);
        }
        return;
    }
    break;

    case Math::CoverageStatus::EXCLUDED:
        // check case where frustum is inside the bounding box 
        /*if ((nearPlaneCenter.x < nodeBound.m_max.x && nearPlaneCenter.y < nodeBound.m_max.y && nearPlaneCenter.z < nodeBound.m_max.z) ||
            (nearPlaneCenter.x > nodeBound.m_min.x && nearPlaneCenter.y > nodeBound.m_min.y && nearPlaneCenter.z > nodeBound.m_min.z))
        {
            nodeCoverage = Math::CoverageStatus::PARTIAL;
            Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
            Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc, nearPlaneCenter, farPlaneCenter);
        }
        else*/
            return;
        break;
    default:
    {
        return;
    }
    break;
    }
}

void Loops::FrustumCuller::AddEntireBVHNode(const Loops::BVHNode* node, const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc)
{
    if(std::holds_alternative<BVHLeafNode>(node->m_node))
    {
        onLeafNodeEntryFunc(node);
        return;
    }
    else
    {
        AddEntireBVHNode(std::get<BVHContainerNode>(node->m_node).m_child0, onLeafNodeEntryFunc);
        AddEntireBVHNode(std::get<BVHContainerNode>(node->m_node).m_child1, onLeafNodeEntryFunc);
        return;
    }
}