#include "Camera.h"

Loops::Camera::Camera(const Loops::Transform& transform, float aspectRatio,
    glm::vec3 up, CameraType projectionType) : m_transform(transform), m_aspect(aspectRatio), m_projectionType(projectionType)
{
    // FOR NOW CAMERA DOESN'T HAS A PARENT
    auto translationMat = glm::translate(m_transform.m_position);
    auto scaleMat = glm::scale(m_transform.m_scale);
    glm::mat4 rotXMat = glm::rotate(m_transform.m_eulerAngles.x, glm::vec3(1, 0, 0));
    glm::mat4 rotYMat = glm::rotate(m_transform.m_eulerAngles.y, glm::vec3(0, 1, 0));
    glm::mat4 rotZMat = glm::rotate(m_transform.m_eulerAngles.z, glm::vec3(0, 0, 1));
    auto rotationMat = rotZMat * rotYMat * rotXMat;

    m_transform.m_modelMat = translationMat * rotationMat * scaleMat;

    auto& matrix = m_transform.m_modelMat;
    glm::vec3 camRight = glm::normalize(glm::vec3(matrix[0][0], matrix[0][1], matrix[0][2]));
    glm::vec3 camUp = glm::normalize(glm::vec3(matrix[1][0], matrix[1][1], matrix[1][2]));
    glm::vec3 camFront = glm::normalize(glm::vec3(matrix[2][0], matrix[2][1], matrix[2][2]));

    glm::vec3 globalPosition = glm::vec3(m_transform.m_modelMat[3]);
    m_viewMat = glm::lookAt(globalPosition, globalPosition + camFront, camUp);

    switch (projectionType)
    {
    case CameraType::ORTHOGONAL:
        //ASSERT_MSG_DEBUG(0, "Need the correct design");
        //projectionMat = glm::ortho( (glm::radians(this->fov), this->aspect, this->zNear, this->zFar);

        break;

    case CameraType::PERSPECTIVE:
        m_projectionMat = glm::perspective(glm::radians(m_fov), m_aspect, m_zNear, m_zFar);
        break;
    }
}

const glm::mat4& Loops::Camera::GetViewMatrix()
{
    return m_viewMat;
}

const glm::mat4& Loops::Camera::GetProjectionMat()
{
    return m_projectionMat;
}

glm::vec3 Loops::Camera::GetPosition()
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