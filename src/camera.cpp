#include "camera.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::ProcessMouse(float dx, float dy, float sensitivity) {
    m_yaw   -= dx * sensitivity;
    m_pitch -= dy * sensitivity;
    // Clamp m_pitch to avoid gimbal lock at poles
    const float limit = glm::radians(89.f);
    m_pitch = glm::clamp(m_pitch, -limit, limit);

    glm::quat qYaw   = glm::angleAxis(m_yaw,   glm::vec3(0, 1, 0));
    glm::quat qPitch = glm::angleAxis(m_pitch, glm::vec3(1, 0, 0));
    m_orientation = glm::normalize(qYaw * qPitch);
}

void Camera::ProcessKeyboard(float fwd, float rgt, float up, float dt, float speed) {
    m_position += Front() * fwd  * speed * dt;
    m_position += Right() * rgt  * speed * dt;
    m_position += glm::vec3(0,1,0) * up * speed * dt;
}

glm::vec3 Camera::Front() const {
    return glm::normalize(m_orientation * glm::vec3(0, 0, -1));
}

glm::vec3 Camera::Right() const {
    return glm::normalize(m_orientation * glm::vec3(1, 0, 0));
}
