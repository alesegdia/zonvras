#include "camera.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::processMouse(float dx, float dy, float sensitivity) {
    yaw   -= dx * sensitivity;
    pitch -= dy * sensitivity;
    // Clamp pitch to avoid gimbal lock at poles
    const float limit = glm::radians(89.f);
    pitch = glm::clamp(pitch, -limit, limit);

    glm::quat qYaw   = glm::angleAxis(yaw,   glm::vec3(0, 1, 0));
    glm::quat qPitch = glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    orientation = glm::normalize(qYaw * qPitch);
}

void Camera::processKeyboard(float fwd, float rgt, float up, float dt, float speed) {
    position += front() * fwd  * speed * dt;
    position += right() * rgt  * speed * dt;
    position += glm::vec3(0,1,0) * up * speed * dt;
}

glm::vec3 Camera::front() const {
    return glm::normalize(orientation * glm::vec3(0, 0, -1));
}

glm::vec3 Camera::right() const {
    return glm::normalize(orientation * glm::vec3(1, 0, 0));
}
