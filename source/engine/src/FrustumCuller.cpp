#include "FrustumCuller.h"
#include "Assertion.h"

void Loops::FrustumCuller::PerformFrustumCulling(const Loops::BVHNode* rootNode, const glm::mat4& viewMat, const glm::mat4& projectionMat,
    const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc)
{
    m_frustum.Update(viewMat, projectionMat);
    Cull(rootNode, m_frustum, Math::CoverageStatus::NUM_STATUS, onLeafNodeEntryFunc);
}

void Loops::FrustumCuller::Cull(const Loops::BVHNode* node, const Math::Frustum& frustum, Math::CoverageStatus parentCoverage,
    const std::function<void(const Loops::BVHNode* rootNode)>& onLeafNodeEntryFunc)
{
    Math::CoverageStatus nodeCoverage{ Math::CoverageStatus::NUM_STATUS };
    const Bounds nodeBound{ node->m_bounds };

    bool isNodeContainer{ false };

    if (std::holds_alternative<BVHContainerNode>(node->m_node))
        isNodeContainer = true;
    else if (std::holds_alternative<BVHLeafNode>(node->m_node))
        isNodeContainer = false;
    else
        ASSERT_MSG(0, "Unassigned");

    auto CheckBoundCoverage = [](const Bounds& bound, const Math::Frustum& frustum) -> Math::CoverageStatus
        {
            return frustum.IsBoxVisible(bound.m_min, bound.m_max);
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
        else
        {
            Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc);
            Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc);
        }
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
            return;

        if (isNodeContainer)
        {
            if (nodeCoverage == Math::CoverageStatus::INCLUDED)
                AddEntireBVHNode(node, onLeafNodeEntryFunc);
            else
            {
                Cull(std::get<BVHContainerNode>(node->m_node).m_child0, frustum, nodeCoverage, onLeafNodeEntryFunc);
                Cull(std::get<BVHContainerNode>(node->m_node).m_child1, frustum, nodeCoverage, onLeafNodeEntryFunc);
            }
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