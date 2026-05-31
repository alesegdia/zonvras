#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera {
public:
    glm::vec3 position{0.f, 0.f, 5.f};
    glm::quat orientation{1.f, 0.f, 0.f, 0.f};

    float yaw   = 0.f;
    float pitch = 0.f;

    void processKeyboard(float forward, float right, float up, float dt, float speed);
    void processMouse(float dx, float dy, float sensitivity = 0.002f);

    glm::vec3 front() const;
    glm::vec3 right() const;
};
