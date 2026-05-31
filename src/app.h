#pragma once
#include "vulkan_context.h"
#include "camera.h"
#include "shader_watcher.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <vector>

struct Example {
    std::string name;      // display name (folder name)
    std::string fragSpv;   // path to compiled .spv
};

class App {
public:
    void run();

private:
    GLFWwindow*   m_window  = nullptr;
    VulkanContext m_vk;
    Camera        m_camera;
    std::unique_ptr<ShaderWatcher> m_fragWatcher;

    std::vector<Example> m_examples;
    int  m_activeExample  = 0;

    float  m_time        = 0.f;
    double m_lastTime    = 0.0;
    double m_lastMouseX  = 0.0;
    double m_lastMouseY  = 0.0;
    bool   m_firstMouse  = true;
    bool   m_captureMouse = false;

    std::string m_vertSpv;

    void initWindow();
    void discoverExamples();
    void switchToExample(int index);
    void mainLoop();
    void cleanup();
    void handleInput(float dt);
    void buildUI();          // ImGui windows

    static void framebufferResizeCallback(GLFWwindow* win, int w, int h);
    static void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* win, int button, int action, int mods);
};
