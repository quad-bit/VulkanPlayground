#include "Camera.h"
#include "Assertion.h"

Loops::Camera::Camera(glm::mat4& transformation, float aspectRatio, glm::vec3 up,
    CameraType projectionType) : m_aspect(aspectRatio), m_projectionType(projectionType)
{
    glm::vec3 camRight = glm::normalize(glm::vec3(transformation[0][0], transformation[0][1], transformation[0][2]));
    glm::vec3 camUp = glm::normalize(glm::vec3(transformation[1][0], transformation[1][1], transformation[1][2]));
    glm::vec3 camFront = glm::normalize(glm::vec3(transformation[2][0], transformation[2][1], transformation[2][2]));

    glm::vec3 globalPosition = glm::vec3(transformation[3]);
    m_viewMat = glm::lookAt(globalPosition, globalPosition + camFront, camUp);

    switch (m_projectionType)
    {
    case CameraType::ORTHOGONAL:
        Loops::ASSERT_MSG_DEBUG(0, "Yet to be implemented");
        //projectionMat = glm::ortho( (glm::radians(this->fov), this->aspect, this->zNear, this->zFar);

        break;

    case CameraType::PERSPECTIVE:
        m_projectionMat = glm::perspective(glm::radians(m_fov), m_aspect, m_zNear, m_zFar);
        break;
    }
}


void Loops::Camera::UpdateCamera(const Loops::Transform& transform)
{
    auto& matrix = transform.m_modelMatGlobal;
    glm::vec3 camRight = glm::normalize(glm::vec3(matrix[0][0], matrix[0][1], matrix[0][2]));
    glm::vec3 camUp = glm::normalize(glm::vec3(matrix[1][0], matrix[1][1], matrix[1][2]));
    glm::vec3 camFront = glm::normalize(glm::vec3(matrix[2][0], matrix[2][1], matrix[2][2]));

    glm::vec3 globalPosition = glm::vec3(matrix[3]);
    m_viewMat = glm::lookAt(globalPosition, globalPosition + camFront, camUp);
}

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