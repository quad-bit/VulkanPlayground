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
            inline bool IsPointVisible(glm::vec3 point) const;
            inline bool IsSphereVisible(glm::vec3 center, float rad) const;
            inline CoverageStatus IsBoxVisible(glm::vec3 min, glm::vec3 max) const;
        };

        bool Loops::Math::Frustum::IsPointVisible(glm::vec3 point) const
        {
            for (int i = 0; i < m_planes.size(); i++)
            {
                if (m_planes[i].GetDistance(point) < 0)
                {
                    return false;
                }
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
            //if (IsPointVisible(min)) return true;
            //if (IsPointVisible(glm::vec3(max.x, min.y, min.z))) return true;
            //if (IsPointVisible(glm::vec3(min.x, max.y, min.z))) return true;
            //if (IsPointVisible(glm::vec3(max.x, max.y, min.z))) return true;
            //if (IsPointVisible(glm::vec3(min.x, min.y, max.z))) return true;
            //if (IsPointVisible(glm::vec3(max.x, min.y, max.z))) return true;
            //if (IsPointVisible(glm::vec3(min.x, max.y, max.z))) return true;
            //if (IsPointVisible(max)) return true;

            unsigned short value = 0; // default not visible
            if (IsPointVisible(min)) value++;
            if (IsPointVisible(max)) value++;
            if (IsPointVisible(glm::vec3(min.x, max.y, max.z))) value++;
            if (IsPointVisible(glm::vec3(max.x, min.y, min.z))) value++;

            CoverageStatus coverage{ CoverageStatus::EXCLUDED };
            if (value > 0)
            {
                coverage = CoverageStatus::PARTIAL;
                if (value == 4)
                    coverage = CoverageStatus::INCLUDED;
            }
            return coverage;
            
        }

    }
}

#endif