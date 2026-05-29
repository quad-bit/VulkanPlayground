#ifndef PLANE_H
#define PLANE_H

#include <glm/glm.hpp>

//using namespace glm;

namespace Loops
{
    namespace Math
    {
        enum POINT_STATE
        {
            PLANE_FRONT,
            PLANE_BACK,
            ON_PLANE,
            NONE
        };
    
        class Plane
        {
        private:
            float a, b, c, d; // ax + by + cz + d = 0
    
        public:
            Plane() : a(0), b(0), c(0), d(0) {}
            Plane(float A, float B, float C, float D) : a(A), b(B), c(C), d(D) {}
            Plane(glm::vec3& p, glm::vec3& q, glm::vec3& r);
            inline void Set(float A, float B, float C, float D);

            inline void Normalize();
            bool Intersect(const glm::vec3& bbmin, const glm::vec3& bbmax) const;// bounding box
            inline bool Intersect(const glm::vec3& center, float beamRadius) const;// with sphere
            inline POINT_STATE ClassifyPoint(const glm::vec3& point, float& dist) const;
            inline float GetDistance(const glm::vec3& point) const;
        };

        bool Loops::Math::Plane::Intersect(const glm::vec3& center, float beamRadius) const
        {
            float dp = glm::abs<float>(GetDistance(center));
            if (dp <= beamRadius)
                return true;

            return false;
        }

        Loops::Math::POINT_STATE Loops::Math::Plane::ClassifyPoint(const glm::vec3& point, float& dist) const
        {
            float dp = glm::abs<float>(GetDistance(point));

            dist = 0;

            if (dp != 0)
                dist = dp;

            if (dp >= 0.001f)
                return Loops::Math::POINT_STATE::PLANE_FRONT;

            if (dp < 0.001f)
                return Loops::Math::POINT_STATE::PLANE_BACK;

            return Loops::Math::POINT_STATE::NONE;
        }

        float Loops::Math::Plane::GetDistance(const glm::vec3& point) const
        {
            return a * point.x + b * point.y + c * point.z + d;
        }

        void Loops::Math::Plane::Set(float A, float B, float C, float D)
        {
            a = A;
            b = B;
            c = C;
            d = D;
        }

        void Loops::Math::Plane::Normalize()
        {
            float length = glm::length(glm::vec3(a, b, c));
            if (length > 0.0f)
            {
                a /= length;
                b /= length;
                c /= length;
                d /= length;
            }
        }

    }
}

#endif