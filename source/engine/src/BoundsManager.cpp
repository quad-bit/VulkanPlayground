#include "BoundsManager.h"
#include "Components.h"
#include "Assertion.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

Loops::BoundsManager::BoundsManager()
{
    m_sceneBound.m_max = glm::vec3(std::numeric_limits<float>::lowest());
    m_sceneBound.m_min = glm::vec3(std::numeric_limits<float>::max());
}

Loops::BoundsManager::~BoundsManager()
{

}

Loops::Bounds Loops::Union(const Loops::Bounds& b1, const Loops::Bounds& b2)
{
    Loops::Bounds bound;
    bound.m_max = glm::vec3(std::max(b1.m_max.x, b2.m_max.x), std::max(b1.m_max.y, b2.m_max.y), std::max(b1.m_max.z, b2.m_max.z));
    bound.m_min = glm::vec3(std::min(b1.m_min.x, b2.m_min.x), std::min(b1.m_min.y, b2.m_min.y), std::min(b1.m_min.z, b2.m_min.z));
    //bound.m_center = (bound.m_max + bound.m_min) / 2.0f;

    return bound;
}

void Loops::BoundsManager::AddBound(const glm::vec3& min, const glm::vec3& max, uint32_t submeshId, uint32_t entityId)
{
    Bounds& bound = m_primitiveBounds[m_primitiveBoundCount];
    bound.m_boundIndex = m_primitiveBoundCount;
    bound.m_max = max;
    bound.m_min = min;

    BoundInfo info{entityId, submeshId};
    m_primitiveBoundInfo.insert({ m_primitiveBoundCount, std::move(info) });
    m_primitiveBoundCount++;
    assert(m_primitiveBoundCount < MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH);

    {
        m_sceneBound.m_max = glm::vec3(std::max(m_sceneBound.m_max.x, bound.m_max.x), std::max(m_sceneBound.m_max.y, bound.m_max.y), std::max(m_sceneBound.m_max.z, bound.m_max.z));
        m_sceneBound.m_min = glm::vec3(std::min(m_sceneBound.m_min.x, bound.m_min.x), std::min(m_sceneBound.m_min.y, bound.m_min.y), std::min(m_sceneBound.m_min.z, bound.m_min.z));
        //m_sceneBound.m_center = (m_sceneBound.m_max + m_sceneBound.m_min) / 2.0f;
    }
}

void Loops::BoundsManager::CalculateSceneBound()
{
    m_sceneBound.m_max = glm::vec3(std::numeric_limits<float>::lowest());
    m_sceneBound.m_min = glm::vec3(std::numeric_limits<float>::max());

    //for (uint32_t i=0; i < m_primitiveBoundCount;i++)
    //{
    //    const Bounds& bound = m_primitiveWorldBounds[i];

    //    //glm::vec3 max = *info.pm_globalMat * glm::vec4(bound.m_max, 1.0f);
    //    //glm::vec3 min = *info.pm_globalMat * glm::vec4(bound.m_min, 1.0f);

    //    m_sceneBound.m_max = glm::vec3(std::max(m_sceneBound.m_max.x, bound.m_max.x), std::max(m_sceneBound.m_max.y, bound.m_max.y), std::max(m_sceneBound.m_max.z, bound.m_max.z));
    //    m_sceneBound.m_min = glm::vec3(std::min(m_sceneBound.m_min.x, bound.m_min.x), std::min(m_sceneBound.m_min.y, bound.m_min.y), std::min(m_sceneBound.m_min.z, bound.m_min.z));
    //    //m_sceneBound.m_center = (m_sceneBound.m_max + m_sceneBound.m_min) / 2.0f;
    //}

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
    *    Bounds m_bounds;
    *    std::variant<LeafNode, ContainerNode> m_node;
    * public:
    *    
    * };
    */

    // retrun the parent node
}

void Loops::BoundsManager::UpdatePrimtiveBoundInWorldSpace(const flecs::world& world)
{
    m_sceneBound.m_max = glm::vec3(std::numeric_limits<float>::lowest());
    m_sceneBound.m_min = glm::vec3(std::numeric_limits<float>::max());

    auto RemoveRotation = [](const glm::mat4& mat) -> glm::mat4
    {
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat rotation;

        // Decompose the matrix into components
        if (!glm::decompose(mat, scale, rotation, translation, skew, perspective))
        {
            ASSERT_MSG(0, "Matrix decomposition failed");
            return glm::mat4(1.0f); // Return identity if failed
        }

        // Rebuild matrix without rotation
        glm::mat4 noRotation = glm::mat4(1.0f);
        noRotation = glm::translate(noRotation, translation);
        noRotation = glm::scale(noRotation, scale);

        return noRotation;
    };


    for (uint32_t i = 0; i < m_primitiveBoundCount; i++)
    {
        const Bounds& bound = m_primitiveBounds[i];
        const BoundInfo& boundInfo = m_primitiveBoundInfo[bound.m_boundIndex];
        Bounds& boundInWorldSpace = m_primitiveWorldBounds[i];

        const glm::mat4& globalMat = world.entity(boundInfo.m_entityId).get<Loops::Transform>().m_modelMatGlobal;

        // removing rotation
        auto matWithoutRotation = RemoveRotation(globalMat);

        boundInWorldSpace.m_max = glm::vec3(matWithoutRotation * glm::vec4(bound.m_max, 1.0f));
        boundInWorldSpace.m_min = glm::vec3(matWithoutRotation * glm::vec4(bound.m_min, 1.0f));

        //boundInWorldSpace.m_max = glm::vec3(globalMat * glm::vec4(bound.m_max, 1.0f));
        //boundInWorldSpace.m_min = glm::vec3(globalMat * glm::vec4(bound.m_min, 1.0f));

        m_sceneBound.m_max = glm::vec3(std::max(m_sceneBound.m_max.x, boundInWorldSpace.m_max.x), std::max(m_sceneBound.m_max.y, boundInWorldSpace.m_max.y), std::max(m_sceneBound.m_max.z, boundInWorldSpace.m_max.z));
        m_sceneBound.m_min = glm::vec3(std::min(m_sceneBound.m_min.x, boundInWorldSpace.m_min.x), std::min(m_sceneBound.m_min.y, boundInWorldSpace.m_min.y), std::min(m_sceneBound.m_min.z, boundInWorldSpace.m_min.z));
    }
}

void Loops::BoundsManager::Update(uint32_t currentFrameInFlight, const flecs::world& world)
{
    UpdatePrimtiveBoundInWorldSpace(world);
    // create BVH
}

std::tuple< const Loops::Bounds*, uint32_t> Loops::BoundsManager::GetPrimitiveBounds() const
{
    return { m_primitiveWorldBounds, m_primitiveBoundCount };
}

const std::unordered_map<uint32_t, Loops::BoundInfo>& Loops::BoundsManager::GetPrimtiveBoundInfo() const
{
    return m_primitiveBoundInfo;
}
