#include "Math/Frustum.h"
#include "Assertion.h"

Loops::Math::Frustum::Frustum()
{
}

Loops::Math::Frustum::~Frustum()
{
}

Loops::Math::Frustum::Frustum(float angle, float ratio, float near, float far, glm::vec3 & camPos, glm::vec3 & lookAtPos, glm::vec3 & up)
{
    ASSERT_MSG(0, "Not being used");
    /*glm::vec3 xVec, yVec, zVec, vecN, vecF;
    glm::vec3 nearTopLeft, nearTopRight, nearBottomLeft, nearBottomRight;
    glm::vec3 farTopLeft, farTopRight, farBottomRight, farBottomLeft;
    float radians = (float)glm::tan(glm::degrees(angle) * 0.5f);
    float nearH = near * radians;
    float nearW = nearH * ratio;
    float farH = far * radians;
    float farW = farH * ratio;

    zVec = camPos - lookAtPos;
    zVec = glm::normalize(zVec);

    xVec = glm::cross(up, zVec);
    xVec = glm::normalize(xVec);

    yVec = glm::cross(zVec, xVec);

    vecN = camPos - zVec * near;
    vecF = camPos - zVec * far;

    nearTopLeft = vecN + yVec * nearH - xVec * nearW;
    nearTopRight = vecN + yVec * nearH + xVec * nearW;
    nearBottomLeft = vecN - yVec * nearH - xVec * nearW;
    nearTopRight = vecN - yVec * nearH + xVec * nearW;

    farTopLeft = vecF + yVec * farH - xVec * farW;
    farTopRight = vecF + yVec * farH + xVec * farW;
    farBottomLeft = vecF - yVec * farH - xVec * farW;
    farTopRight = vecF - yVec * farH + xVec * farW;

    Plane planeObj;

    planeObj.Create(nearTopRight, nearTopLeft, farTopLeft);
    m_planes[(planeObj);

    planeObj.Create(nearBottomLeft, nearBottomRight, farBottomRight);
    AddPlane(planeObj);

    planeObj.Create(nearTopLeft, nearBottomLeft, farBottomLeft);
    AddPlane(planeObj);

    planeObj.Create(nearBottomRight, nearTopRight, farBottomRight);
    AddPlane(planeObj);

    planeObj.Create(nearTopLeft, nearTopRight, nearBottomRight);
    AddPlane(planeObj);

    planeObj.Create(farTopRight, farTopLeft, farBottomLeft);
    AddPlane(planeObj);*/
}

Loops::Math::Frustum::Frustum(const glm::mat4& viewMat, const glm::mat4& projectionMat)
{
    const glm::mat4 vp = projectionMat * viewMat;

    // Left
    m_planes[LEFT] = std::move(Plane(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]));
    m_planes[LEFT].Normalize();
    //m_planes[LEFT].a = vp[0][3] + vp[0][0];
    //m_planes[LEFT].b = vp[1][3] + vp[1][0];
    //m_planes[LEFT].c = vp[2][3] + vp[2][0];
    //m_planes[LEFT].d = vp[3][3] + vp[3][0];
    //normalizePlane(m_planes[LEFT]);

    // Right
    m_planes[RIGHT] = std::move(Plane(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]));
    m_planes[RIGHT].Normalize();

    //m_planes[RIGHT].normal.x = vp[0][3] - vp[0][0];
    //m_planes[RIGHT].normal.y = vp[1][3] - vp[1][0];
    //m_planes[RIGHT].normal.z = vp[2][3] - vp[2][0];
    //m_planes[RIGHT].d = vp[3][3] - vp[3][0];
    //normalizePlane(m_planes[RIGHT]);

    // Bottom
    m_planes[BOTTOM] = std::move(Plane(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]));
    m_planes[BOTTOM].Normalize();

    //m_planes[BOTTOM].normal.x = vp[0][3] + vp[0][1];
    //m_planes[BOTTOM].normal.y = vp[1][3] + vp[1][1];
    //m_planes[BOTTOM].normal.z = vp[2][3] + vp[2][1];
    //m_planes[BOTTOM].d = vp[3][3] + vp[3][1];
    //normalizePlane(m_planes[BOTTOM]);

    // Top
    m_planes[TOP] = std::move(Plane(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]));
    m_planes[TOP].Normalize();

    //m_planes[TOP].normal.x = vp[0][3] - vp[0][1];
    //m_planes[TOP].normal.y = vp[1][3] - vp[1][1];
    //m_planes[TOP].normal.z = vp[2][3] - vp[2][1];
    //m_planes[TOP].d = vp[3][3] - vp[3][1];
    //normalizePlane(m_planes[TOP]);

    // Near
    m_planes[NEAR_PLANE] = std::move(Plane(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]));
    m_planes[NEAR_PLANE].Normalize();

    //m_planes[NEAR_PLANE].normal.x = vp[0][3] + vp[0][2];
    //m_planes[NEAR_PLANE].normal.y = vp[1][3] + vp[1][2];
    //m_planes[NEAR_PLANE].normal.z = vp[2][3] + vp[2][2];
    //m_planes[NEAR_PLANE].d = vp[3][3] + vp[3][2];
    //normalizePlane(m_planes[NEAR_PLANE]);

    // Far
    m_planes[FAR_PLANE] = std::move(Plane(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]));
    m_planes[FAR_PLANE].Normalize();

    //m_planes[FAR_PLANE].normal.x = vp[0][3] - vp[0][2];
    //m_planes[FAR_PLANE].normal.y = vp[1][3] - vp[1][2];
    //m_planes[FAR_PLANE].normal.z = vp[2][3] - vp[2][2];
    //m_planes[FAR_PLANE].d = vp[3][3] - vp[3][2];
    //normalizePlane(m_planes[FAR_PLANE]);
}

void Loops::Math::Frustum::Update(const glm::mat4& viewMat, const glm::mat4& projectionMat)
{
    const glm::mat4 vp = projectionMat * viewMat;

    // Left
    m_planes[LEFT].Set(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    m_planes[LEFT].Normalize();
    
    // Right
    m_planes[RIGHT].Set(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    m_planes[RIGHT].Normalize();

    // Bottom
    m_planes[BOTTOM].Set(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    m_planes[BOTTOM].Normalize();

    // Top
    m_planes[TOP].Set(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    m_planes[TOP].Normalize();

    // Near
    m_planes[NEAR_PLANE].Set(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    m_planes[NEAR_PLANE].Normalize();

    // Far
    m_planes[FAR_PLANE].Set(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    m_planes[FAR_PLANE].Normalize();
}

