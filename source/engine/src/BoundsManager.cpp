#include "BoundsManager.h"
#include "Components.h"
#include "Assertion.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>


namespace
{
    Loops::Bounds Union(const Loops::Bounds& b1, const Loops::Bounds& b2)
    {
        Loops::Bounds bound;
        bound.m_max = glm::vec3(std::max(b1.m_max.x, b2.m_max.x), std::max(b1.m_max.y, b2.m_max.y), std::max(b1.m_max.z, b2.m_max.z));
        bound.m_min = glm::vec3(std::min(b1.m_min.x, b2.m_min.x), std::min(b1.m_min.y, b2.m_min.y), std::min(b1.m_min.z, b2.m_min.z));

        return bound;
    }

    Loops::Bounds Union(const Loops::Bounds& b1, glm::vec3 point)
    {
        Loops::Bounds bound;
        bound.m_max = glm::vec3(std::max(b1.m_max.x, point.x), std::max(b1.m_max.y, point.y), std::max(b1.m_max.z, point.z));
        bound.m_min = glm::vec3(std::min(b1.m_min.x, point.x), std::min(b1.m_min.y, point.y), std::min(b1.m_min.z, point.z));

        return bound;
    }

    uint8_t MaximumExtent(const Loops::Bounds& bound)
    {
        glm::vec3 diagonal = bound.m_max - bound.m_min;
        if (diagonal.x > diagonal.y && diagonal.x > diagonal.z)
            return 0;
        else if (diagonal.y > diagonal.z)
            return 1;
        else
            return 2;

        Loops::ASSERT_MSG(0, "Something wrong");
        return 0;
    }

    float SurfaceArea(const Loops::Bounds& bound)
    {
        glm::vec3 diagonal = bound.m_max - bound.m_min;
        return 2 * (diagonal.x * diagonal.y + diagonal.x * diagonal.z + diagonal.y * diagonal.z);
    }

    float Volume(const Loops::Bounds& bound)
    {
        glm::vec3 diagonal = bound.m_max - bound.m_min;
        return (diagonal.x * diagonal.y * diagonal.z);
    }

    glm::vec3 Offset(const Loops::Bounds& bound, const glm::vec3& point)
    {
        glm::vec3 o = point - bound.m_min;
        if (bound.m_max.x > bound.m_min.x)
            o.x /= bound.m_max.x - bound.m_min.x;
        if (bound.m_max.y > bound.m_min.y)
            o.y /= bound.m_max.y - bound.m_min.y;
        if (bound.m_max.z > bound.m_min.z)
            o.z /= bound.m_max.z - bound.m_min.z;
        return o;
    }
}

void Loops::BVHNode::InitLeaf(uint32_t startIndex, uint32_t numBounds, const Bounds& bound)
{
    BVHLeafNode leaf{ startIndex, numBounds };
    m_node = leaf;
    m_bounds = bound;
}

void Loops::BVHNode::InitContainer(uint8_t axis, BVHNode* child0, BVHNode* child1)
{
    BVHContainerNode container{ axis, child0, child1 };
    m_node = container;

    m_bounds = Union((child0->m_bounds), (child1->m_bounds));
}

Loops::BoundsManager::BoundsManager()
{
    m_sceneBound.m_max = glm::vec3(std::numeric_limits<float>::lowest());
    m_sceneBound.m_min = glm::vec3(std::numeric_limits<float>::max());
}

Loops::BoundsManager::~BoundsManager()
{

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

    glm::vec3 min (std::numeric_limits<float>::max());
    glm::vec3 max (std::numeric_limits<float>::lowest());

    // the original bounding box corner, but it get transformed into some other corner 
    // but since we put same transformation on other two corners it evens out
    glm::vec3 corner0, corner1;
    glm::vec3 corner2, corner3;

    for (uint32_t i = 0; i < m_primitiveBoundCount; i++)
    {
        const Bounds& bound = m_primitiveBounds[i];
        const BoundInfo& boundInfo = m_primitiveBoundInfo[bound.m_boundIndex];
        Bounds& boundInWorldSpace = m_primitiveWorldBounds[i];

        const glm::mat4& globalMat = world.entity(boundInfo.m_entityId).get<Loops::Transform>().m_modelMatGlobal;

        corner0 = globalMat * glm::vec4(bound.m_max, 1.0f);
        corner1 = globalMat * glm::vec4(bound.m_min, 1.0f);

        corner2 = glm::vec3(bound.m_min.x, bound.m_max.y, bound.m_max.z);
        corner3 = glm::vec3(bound.m_max.x, bound.m_min.y, bound.m_min.z);

        corner2 = globalMat * glm::vec4(corner2, 1.0f);
        corner3 = globalMat * glm::vec4(corner3, 1.0f);

        min = glm::vec3(std::numeric_limits<float>::max());
        max = glm::vec3(std::numeric_limits<float>::lowest());

        min = glm::min(corner0, min);
        min = glm::min(corner1, min);
        min = glm::min(corner2, min);
        min = glm::min(corner3, min);

        max = glm::max(corner0, max);
        max = glm::max(corner1, max);
        max = glm::max(corner2, max);
        max = glm::max(corner3, max);

        boundInWorldSpace.m_max = max;
        boundInWorldSpace.m_min = min;
    }
}

void Loops::BoundsManager::Update(uint32_t currentFrameInFlight, const flecs::world& world)
{
    UpdatePrimtiveBoundInWorldSpace(world);

    // create BVH
    m_bvhNodeCount = 0;
    uint32_t totalNodeCount = 0;

    if (m_activeCreationMethod == BvhCreationMethod::RECURSIVE)
        m_bvhRootNode = BuildBVHRecursive(m_primitiveWorldBounds, m_primitiveBoundCount, 0, m_primitiveBoundCount - 1, totalNodeCount, m_primitiveBoundIndicies, m_primitiveBoundIndiciesCount,
            m_activeSplitMethod);
    else
        ASSERT_MSG(0, "Coming soon");
}

std::tuple< const Loops::Bounds*, uint32_t> Loops::BoundsManager::GetPrimitiveBounds() const
{
    return { m_primitiveWorldBounds, m_primitiveBoundCount };
}

std::tuple< const Loops::Bounds*, uint32_t> Loops::BoundsManager::GetBvhNodeBounds() const
{
    uint32_t i = 0;
    for (auto& node : m_bvhNodeArray)
    {
        m_bvhNodeBoundArray[i++] = node.m_bounds;
    }
    return { m_bvhNodeBoundArray, m_bvhNodeCount };
}

const std::unordered_map<uint32_t, Loops::BoundInfo>& Loops::BoundsManager::GetPrimtiveBoundInfo() const
{
    return m_primitiveBoundInfo;
}

Loops::BVHNode& Loops::BoundsManager::GetBvhNode()
{
    Loops::BVHNode& node = m_bvhNodeArray[m_bvhNodeCount];
    node.m_bounds.m_boundIndex = m_bvhNodeCount;
    m_bvhNodeCount++;
    ASSERT_MSG(m_bvhNodeCount < NUM_BVH_NODES," Node count exceeded");
    return node;
}

Loops::BVHNode* Loops::BoundsManager::BuildBVHRecursive(Bounds* primitiveBoundsArray, uint32_t numPrimitiveBounds, uint32_t start, uint32_t end,
    uint32_t& totalNodes, uint32_t* orderedBoundsArrayIndiciesArray, uint32_t& orderedBoundIndiciesCount, const SplitMethod& splitMethod)
{
    BVHNode& node = GetBvhNode();
    totalNodes++;

    struct BVHData
    {
        Bounds m_nodeBounds{};
        Bounds m_centroidBounds{};
    };

    auto CalculateInitialData = [&primitiveBoundsArray, &numPrimitiveBounds](uint32_t start, uint32_t end) -> BVHData
        {
            Bounds nodeBounds{};
            Bounds centroidBounds{};

            for (uint32_t i = start; i <= end; i++)
            {
                nodeBounds = Union(nodeBounds, primitiveBoundsArray[i]);
                centroidBounds = Union(centroidBounds, (primitiveBoundsArray[i].m_max + primitiveBoundsArray[i].m_min) / 2.0f);
            }

            BVHData data{ nodeBounds, centroidBounds };
            return data;
        };

    auto CreateLeaf = [&orderedBoundIndiciesCount, &orderedBoundsArrayIndiciesArray, &primitiveBoundsArray, &node](uint32_t start,
        uint32_t end , uint32_t numBoundsInNode, Bounds nodeBounds)
        {
            for (uint32_t i = start; i <= end; i++)
            {
                orderedBoundsArrayIndiciesArray[orderedBoundIndiciesCount++] = primitiveBoundsArray[i].m_boundIndex;
            }
            node.InitLeaf(start, numBoundsInNode, nodeBounds);
        };

    const BVHData bvhData = CalculateInitialData(start, end);

    uint32_t numBoundsInNode = end - start + 1;
    uint32_t firstBoundIndex = orderedBoundIndiciesCount;

    // Create leaf
    if (numBoundsInNode <= MAX_PRIMITIVES_PER_LEAF)
    {
        CreateLeaf(start, end, numBoundsInNode, bvhData.m_nodeBounds);
        return &node;
    }
    else // Create container node
    {
        const uint8_t dim = MaximumExtent(bvhData.m_centroidBounds);
        uint32_t mid = (start + end) / 2;

        if (bvhData.m_centroidBounds.m_max[dim] == bvhData.m_centroidBounds.m_min[dim])
        {
            CreateLeaf(start, end, numBoundsInNode, bvhData.m_nodeBounds);
            return &node;
        }
        else
        {
            switch (splitMethod)
            {
                case SplitMethod::MID:
                {
                    float midPoint = (bvhData.m_centroidBounds.m_min[dim] + bvhData.m_centroidBounds.m_max[dim]) / 2.0f;
                    Bounds* midPtr = std::partition(&primitiveBoundsArray[start], &primitiveBoundsArray[end], [dim, midPoint](const Bounds& bound)
                        {
                            const glm::vec3 centroid = (bound.m_max + bound.m_min) / 2.0f;
                            return centroid[dim] < midPoint;
                        });
                    mid = midPtr - &primitiveBoundsArray[start];
                    // so just in case mid reaches end or start and primitive count is more than needed to create leaf we go to equal count
                    if (mid > start && mid < end)//mid != start && mid != end && 
                        break;
                }
                case SplitMethod::EQUAL_COUNT:
                    mid = (start + end) / 2;
                    std::nth_element(&primitiveBoundsArray[start], &primitiveBoundsArray[mid], &primitiveBoundsArray[end], [dim](const Bounds& b1, const Bounds& b2)
                        {
                            const glm::vec3 centroid1 = (b1.m_max + b1.m_min) / 2.0f;
                            const glm::vec3 centroid2 = (b2.m_max + b2.m_min) / 2.0f;

                            return centroid1[dim] < centroid2[dim];
                        });
                    break;

                case SplitMethod::SAH:
                    if (numBoundsInNode <= 4)
                    {
                        mid = (start + end) / 2;
                        std::nth_element(&primitiveBoundsArray[start], &primitiveBoundsArray[mid], &primitiveBoundsArray[end], [dim](const Bounds& b1, const Bounds& b2)
                            {
                                const glm::vec3 centroid1 = (b1.m_max + b1.m_min) / 2.0f;
                                const glm::vec3 centroid2 = (b2.m_max + b2.m_min) / 2.0f;

                                return centroid1[dim] < centroid2[dim];
                            });
                    }
                    else
                    {
                        constexpr int numBuckets = 12;
                        struct BucketInfo
                        {
                            int m_count = 0;
                            Bounds bound;
                        } buckets[numBuckets];

                        for (uint32_t i = start; i <= end; i++)
                        {
                            glm::vec3 centroid = (primitiveBoundsArray[i].m_max + primitiveBoundsArray[i].m_min) / 2.0f;

                            // get a normalised value and then multiply with numBuckets to get the bucket value
                            // in the axis with maximum extent
                            int bucketIndex = numBuckets * Offset(bvhData.m_centroidBounds, centroid)[dim];
                            if (bucketIndex == numBuckets)
                                bucketIndex = numBuckets - 1;
                            buckets[bucketIndex].m_count++;
                            buckets[bucketIndex].bound = Union(buckets[bucketIndex].bound, primitiveBoundsArray[i]);
                        }

                        // cost calculation
                        float cost[numBuckets - 1];
                        float minCost = std::numeric_limits<float>::max();
                        uint32_t minCostSplitBucketIndex = 0;
                        for (uint16_t i = 0; i < numBuckets - 1; i++)
                        {
                            Bounds b0, b1;
                            uint32_t count0 = 0, count1 = 0;

                            // Create bucket from 0-k and k-numBuckets, move k gradually from 1 to numBuckets - 1,
                            // the last bucket will not have the boundary
                            for (uint32_t j = 0; j <= i; j++)
                            {
                                b0 = Union(b0, buckets[j].bound);
                                count0 += buckets[j].m_count;
                            }

                            for (uint32_t j = i + 1; j < numBuckets - 1; j++)
                            {
                                b1 = Union(b1, buckets[j].bound);
                                count1 += buckets[j].m_count;
                            }

                            // cost ray travelling through the node
                            // estimated intersection cost set to 1 and then traversal cost is set to 1/8
                            // its always measured in relative terms rather than absolute but absolute values 
                            // can be calculated and used 
                            const float traversalCost = 0.125f;

                            // probability of intersection with primitives within node will be directly proportional to the surface area of bucket divided by 
                            // area of centeroid bounds (from start to end)
                            cost[i] = traversalCost + (count0 * SurfaceArea(b0) + count1 * SurfaceArea(b1))/SurfaceArea(bvhData.m_centroidBounds);
                            if (cost[i] < minCost)
                            {
                                minCost = cost[i];
                                minCostSplitBucketIndex = i;
                            }
                        }

                        float leafCost = numBoundsInNode;
                        if (numBoundsInNode > MAX_PRIMITIVES_PER_LEAF || minCost < leafCost)
                        {
                            // split primitives based on the container bucket
                            Bounds* midPtr = std::partition(&primitiveBoundsArray[start], &primitiveBoundsArray[end], 
                                [dim, &bvhData, &minCostSplitBucketIndex](const Bounds& bound)
                                {
                                    const glm::vec3 centroid = (bound.m_max + bound.m_min) / 2.0f;
                                    int bucketIndex = numBuckets * Offset(bvhData.m_centroidBounds, centroid)[dim];
                                    if (bucketIndex == numBuckets)
                                        bucketIndex = numBuckets - 1;
                                    return bucketIndex <= minCostSplitBucketIndex;
                                });

                            mid = midPtr - &primitiveBoundsArray[start];
                        }
                        else
                        {
                            // create leaf node
                            CreateLeaf(start, end, numBoundsInNode, bvhData.m_nodeBounds);
                            return &node;
                        }
                    }
                    break;
                default:
                    break;
            }

            node.InitContainer(dim,
                BuildBVHRecursive(primitiveBoundsArray, numPrimitiveBounds, start, mid, totalNodes, orderedBoundsArrayIndiciesArray, orderedBoundIndiciesCount, splitMethod),
                BuildBVHRecursive(primitiveBoundsArray, numPrimitiveBounds, mid + 1, end, totalNodes, orderedBoundsArrayIndiciesArray, orderedBoundIndiciesCount, splitMethod));
        }
    }
    return &node;
}

void Loops::BoundsManager::SetSplitType(const Loops::SplitMethod& splitMethod)
{
    m_activeSplitMethod = splitMethod;
}

void Loops::BoundsManager::SetCeationMethod(const Loops::BvhCreationMethod& creationMethod)
{
    m_activeCreationMethod = creationMethod;
}

const Loops::SplitMethod& Loops::BoundsManager::GetSplitType() const
{
    return m_activeSplitMethod;
}

const Loops::BvhCreationMethod& Loops::BoundsManager::GetCreationMethod() const
{
    return m_activeCreationMethod;
}