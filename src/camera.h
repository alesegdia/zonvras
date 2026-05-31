#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera {
public:
    glm::vec3 m_position{0.f, 0.f, 5.f};
    glm::quat m_orientation{1.f, 0.f, 0.f, 0.f};

    float m_yaw   = 0.f;
    float m_pitch = 0.f;

    void ProcessKeyboard(float forward, float Right, float up, float dt, float speed);
    void ProcessMouse(float dx, float dy, float sensitivity = 0.002f);

    glm::vec3 Front() const;
    glm::vec3 Right() const;
};
