#include "Math/Plane.h"


Loops::Math::Plane::Plane(glm::vec3 & p, glm::vec3 & q, glm::vec3 & r)
{
    glm::vec3 e1, e2, n;
    e1 = q - p;
    e2 = r - p;
    n = glm::cross(e1, e2);
    n = glm::normalize(n);

    a = n.x; b = n.y; c = n.z;
    d = -(a * p.x + b * p.y + c * p.z);
}


bool Loops::Math::Plane::Intersect(const glm::vec3 & bbmin, const glm::vec3 & bbmax) const
{
    glm::vec3 min, max;
    glm::vec3 normal(a, b, c);

    if (normal.x >= 0.0f)
    {
        min.x = bbmin.x;
        max.x = bbmax.x;
    }
    else
    {
        min.x = bbmax.x;
        max.x = bbmin.x;
    }

    if (normal.y >= 0.0f)
    {
        min.y = bbmin.y;
        max.y = bbmax.y;
    }
    else
    {
        min.y = bbmax.y;
        max.y = bbmin.y;
    }

    if (normal.z >= 0.0f)
    {
        min.z = bbmin.z;
        max.z = bbmax.z;
    }
    else
    {
        min.z = bbmax.z;
        max.z = bbmin.z;
    }

    if ((glm::dot(normal, min) + d) > 0.0f)
        return false;

    if ((glm::dot(normal, max) + d) >= 0.0f)
        return true;

    return false;
}


