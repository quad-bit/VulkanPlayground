#include "BoundsManager.h"


Common::AABB Common::Union(const Common::AABB& b1, const Common::AABB& b2)
{
    Common::AABB bound;
    bound.m_max = glm::vec3(std::max(b1.m_max.x, b2.m_max.x), std::max(b1.m_max.y, b2.m_max.y), std::max(b1.m_max.z, b2.m_max.z));
    bound.m_min = glm::vec3(std::min(b1.m_min.x, b2.m_min.x), std::min(b1.m_min.y, b2.m_min.y), std::min(b1.m_min.z, b2.m_min.z));
    bound.m_center = (bound.m_max + bound.m_min) / 2.0f;

    return bound;
}

void Common::BoundsManager::AddBound(const glm::vec3& min, const glm::vec3& max, glm::mat4* globalMat, uint32_t submeshId, uint32_t entityId)
{
    AABB& bound = m_bounds[m_primitiveBoundCount];
    bound.m_boundIndex = m_primitiveBoundCount;

    auto& info = m_infos[m_primitiveBoundCount];
    info.m_boundIndex = m_primitiveBoundCount;
    
    m_primitiveBoundCount++;
    assert(m_primitiveBoundCount < MAX_BOUNDS);

    bound.m_center = (min + max) / 2.0f;
    bound.m_min = min;
    bound.m_max = max;

    info.m_entityId = entityId;
    info.m_submeshId = submeshId;
    info.pm_globalMat = globalMat;

    {
        m_sceneBound.m_max = glm::vec3(std::max(m_sceneBound.m_max.x, bound.m_max.x), std::max(m_sceneBound.m_max.y, bound.m_max.y), std::max(m_sceneBound.m_max.z, bound.m_max.z));
        m_sceneBound.m_min = glm::vec3(std::min(m_sceneBound.m_min.x, bound.m_min.x), std::min(m_sceneBound.m_min.y, bound.m_min.y), std::min(m_sceneBound.m_min.z, bound.m_min.z));
        m_sceneBound.m_center = (m_sceneBound.m_max + m_sceneBound.m_min) / 2.0f;
    }
}

void Common::BoundsManager::CalculateSceneBound()
{
    for (uint32_t i=0; i < m_primitiveBoundCount;i++)
    {
        const BoundInfo& info = m_infos[i];
        const auto& bound = m_bounds[info.m_boundIndex];

        glm::vec3 max = *info.pm_globalMat * glm::vec4(bound.m_max, 1.0f);
        glm::vec3 min = *info.pm_globalMat * glm::vec4(bound.m_min, 1.0f);

        m_sceneBound.m_max = glm::vec3(std::max(m_sceneBound.m_max.x, bound.m_max.x), std::max(m_sceneBound.m_max.y, bound.m_max.y), std::max(m_sceneBound.m_max.z, bound.m_max.z));
        m_sceneBound.m_min = glm::vec3(std::min(m_sceneBound.m_min.x, bound.m_min.x), std::min(m_sceneBound.m_min.y, bound.m_min.y), std::min(m_sceneBound.m_min.z, bound.m_min.z));
        m_sceneBound.m_center = (m_sceneBound.m_max + m_sceneBound.m_min) / 2.0f;
    }

    // Create L_B_V_H

    /*
    * struct LeafNode
    * {
    *    uint32_t m_splitAxis, m_firstPrimitiveIndex, m_numPrimitives;
    * };
    * 
    * struct ContainerNode
    * {
    *    TreeNode* m_children[2]{nullptr, nullptr};
    * };
    * 
    * 
    * class TreeNode
    * {
    * private:
    *    AABB m_bounds;
    *    std::variant<LeafNode, ContainerNode> m_node;
    * public:
    *    
    * };
    */

    // retrun the parent node
}

void Common::BoundsManager::Update(uint32_t currentFrameInFlight)
{
    CalculateSceneBound();
}
