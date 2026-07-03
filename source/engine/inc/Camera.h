#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include "Components.h"

namespace Loops
{
    enum class CameraType
    {
        PERSPECTIVE,
        ORTHOGONAL
    };

    class Camera
    {
        glm::vec3 m_worldUp;
        glm::mat4 m_viewMat;
        glm::mat4 m_projectionMat;

        float m_fov = 45.0f;
        float m_aspect = 1.0f;
        float m_zNear = 0.20f, m_zFar = 500.0f;
        CameraType m_projectionType;

        Camera() = delete;

    public:
        Camera(glm::mat4& transformation, float aspectRatio, float near = 0.5f, float far = 500.0f, float fov = 60.0f,
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
            CameraType projectionType = CameraType::PERSPECTIVE);

        inline const glm::mat4& GetViewMatrix() const;
        inline const glm::mat4& GetProjectionMat() const;
        inline const float GetNearZ() const;
        inline const float GetFarZ() const;
        inline const float GetFov() const;

        void UpdateCamera(const Transform& transform);
    };

    inline const glm::mat4& Loops::Camera::GetViewMatrix() const
    {
        return m_viewMat;
    }

    inline const glm::mat4& Loops::Camera::GetProjectionMat() const
    {
        return m_projectionMat;
    }

    inline const float Loops::Camera::GetNearZ() const
    {
        return m_zNear;
    }

    inline const float Loops::Camera::GetFarZ() const
    {
        return m_zFar;
    }

    inline const float Loops::Camera::GetFov() const
    {
        return m_fov;
    }
}

#endif