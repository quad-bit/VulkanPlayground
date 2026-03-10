#include "Camera.h"

Common::Camera::Camera(const Common::Transform& transform, float aspectRatio,
    glm::vec3 up, CameraType projectionType)
{
    auto matrix = m_transform.m_modelMatGlobal;
    glm::vec3 camRight = glm::normalize(glm::vec3(matrix[0][0], matrix[0][1], matrix[0][2]));
    glm::vec3 camUp = glm::normalize(glm::vec3(matrix[1][0], matrix[1][1], matrix[1][2]));
    glm::vec3 camFront = glm::normalize(glm::vec3(matrix[2][0], matrix[2][1], matrix[2][2]));

    glm::vec3 globalPosition = glm::vec3(m_transform.m_modelMatGlobal[3]);
    m_viewMat = glm::lookAt(globalPosition, globalPosition + camFront, camUp);

    switch (projectionType)
    {
    case CameraType::ORTHOGONAL:
        //ASSERT_MSG_DEBUG(0, "Need the correct design");
        //projectionMat = glm::ortho( (glm::radians(this->fov), this->aspect, this->zNear, this->zFar);

        break;

    case CameraType::PERSPECTIVE:
        m_projectionMat = glm::perspective(glm::radians(this->fov), m_aspect, m_zNear, m_zFar);
        break;
    }
}

const glm::mat4& Common::Camera::GetViewMatrix()
{
    return m_viewMat;
}

const glm::mat4& Common::Camera::GetProjectionMat()
{
    return m_projectionMat;
}

glm::vec3 Common::Camera::GetPosition()
{
    return m_transform.m_position;
}

//
//void Camera::UpdateLocalAxes()
//{
//    glm::vec3 front;
//    front.x = cos(glm::radians(transform.GetLocalEulerAngles().y)) * cos(glm::radians(transform.GetLocalEulerAngles().x));
//    front.y = sin(glm::radians(transform.GetLocalEulerAngles().x));
//    front.z = sin(glm::radians(transform.GetLocalEulerAngles().y)) * cos(glm::radians(transform.GetLocalEulerAngles().x));
//    glm::vec3 camFront = glm::normalize(front);
//    // also re-calculate the Right and Up vector
//    glm::vec3 camRight = glm::normalize(glm::cross(camFront, worldUp));
//    glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFront));
//}