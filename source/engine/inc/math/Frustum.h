#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "Plane.h"
#include <glm\glm.hpp>
#include <array>

namespace Loops
{
    namespace Math
    {
        enum class CoverageStatus
        {
            INCLUDED,
            EXCLUDED,
            PARTIAL,
            NUM_STATUS
        };

        class Frustum
        {
        private:
            std::array<Plane, 6> m_planes;

            enum PLANE_TYPE
            {
                LEFT = 0,
                RIGHT,
                BOTTOM,
                TOP,
                NEAR_PLANE, // collision
                FAR_PLANE
            };

        public:
            Frustum();
            ~Frustum();

            Frustum(float angle, float ratio, float near, float far, glm::vec3& camPos, glm::vec3& lookAtPos, glm::vec3& up);
            Frustum(const glm::mat4& viewMat, const glm::mat4& projectionMat);
            void Update(const glm::mat4& viewMat, const glm::mat4& projectionMat);

            std::pair<uint8_t, unsigned short int> CheckPointVisibility(const glm::vec3& point) const;
            inline bool IsPointVisible(glm::vec3 point) const;
            inline bool IsSphereVisible(glm::vec3 center, float rad) const;
            inline CoverageStatus IsBoxVisible(glm::vec3 min, glm::vec3 max) const;
        };

        bool Loops::Math::Frustum::IsPointVisible(glm::vec3 point) const
        {
            /*for (int i = 0; i < m_planes.size(); i++)
            {
                float distance{ 0.0f };
                auto location = m_planes[i].ClassifyPoint(point);
                if (location == POINT_STATE::PLANE_BACK)
                {
                    return false;
                }
            }
            return true;*/

            for (int i = 0; i < m_planes.size(); i++)
            {
                glm::vec3 normal{ m_planes[i].a , m_planes[i].b, m_planes[i].c };
                if ((glm::dot(normal, point) + m_planes[i].d) < 0)
                    return false;
            }
            return true;
        }

        bool Loops::Math::Frustum::IsSphereVisible(glm::vec3 center, float rad) const
        {
            float distance = 0;

            for (int i = 0; i < m_planes.size(); i++)
            {
                distance = m_planes[i].GetDistance(center);
                if (distance < -rad)
                    return false;
            }
            return true;
        }

        CoverageStatus Loops::Math::Frustum::IsBoxVisible(glm::vec3 min, glm::vec3 max) const
        {
            //unsigned short value = 0; // default not visible
            //if (IsPointVisible(min)) value++;
            //if (IsPointVisible(max)) value++;
            //if (IsPointVisible(glm::vec3(min.x, max.y, max.z))) value++;
            //if (IsPointVisible(glm::vec3(max.x, min.y, min.z))) value++;

            //CoverageStatus coverage{ CoverageStatus::EXCLUDED };
            //if (value > 0)
            //{
            //    coverage = CoverageStatus::PARTIAL;
            //    if (value == 4)
            //        coverage = CoverageStatus::INCLUDED;
            //}
            //return coverage;

            auto [minMask, numZerosInMinMask] = CheckPointVisibility(min);
            auto [maxMask, numZerosInMaxMask] = CheckPointVisibility(max);

            // 0b00111111 = 26 - 1 = 63
            // both min max are within all planes
            if (minMask == 63 && maxMask == 63)
            {
                return CoverageStatus::INCLUDED;
            }
            // either min or max is within all planes
            if (minMask == 63 || maxMask == 63)
            {
                return CoverageStatus::PARTIAL;
            }
#if 0
            /*if(numZerosInMaxMask < 2 || numZerosInMinMask < 2)
                return CoverageStatus::PARTIAL;
            else 
                return CoverageStatus::EXCLUDED;*/
#else
            // check for edge cases
            {
                // both min and max is 0 for any plane
                const uint8_t zeroOrCheck = minMask | maxMask;
                const uint8_t zeroAndCheck = minMask & maxMask;
                uint8_t numZerosInOr = 0;
                for (unsigned short i = 0; i < 6; i++)
                {
                    if ((zeroOrCheck & (1U << i)) == 0)
                    {
                        numZerosInOr++;
                        // test bounding sphere against ith plane
                        /*const glm::vec3 center = (min + max) / 2.0f;
                        const float radius = glm::distance(min, max);
                        if (m_planes[i].Intersect(center, radius))
                            return CoverageStatus::PARTIAL;
                        else*/
                            //return CoverageStatus::EXCLUDED;
                    }
                }
                if(numZerosInOr <= 1)
                    return CoverageStatus::PARTIAL;
                else
                    return CoverageStatus::EXCLUDED;
            }
#endif
            return CoverageStatus::PARTIAL;
        }

    }
}

#endif