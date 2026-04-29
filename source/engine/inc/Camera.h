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

    struct SceneUniform
    {
        glm::mat4 viewMat;
        glm::mat4 projectionMat;
        glm::vec3 cameraPos;
    };

    class Camera
    {
        glm::vec3 m_worldUp;
        glm::mat4 m_viewMat;
        glm::mat4 m_projectionMat;

        float m_fov = 60.0f;
        float m_aspect = 1.0f;
        float m_zNear = 0.20f, m_zFar = 1500.0f;
        CameraType m_projectionType;

        Camera() = delete;

    public:

        Loops::Transform m_transform;
        // Constructor with vectors
        Camera(const Loops::Transform& transform, float aspectRatio,
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
            CameraType projectionType = CameraType::PERSPECTIVE);

        const glm::mat4& GetViewMatrix();
        const glm::mat4& GetProjectionMat();
        glm::vec3 GetPosition();

    };
}

#endif