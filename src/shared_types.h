#pragma once
#include <glm/glm.hpp>

// Uniform block sent to the fragment shader every frame
struct PushConstants {
    float     time;
    float     pad0;
    float     pad1;
    float     pad2;
    glm::vec2 resolution;
    glm::vec2 pad3;
    glm::vec3 camPos;
    float     pad4;
    glm::vec4 camRot; // quaternion (w, x, y, z)
};
