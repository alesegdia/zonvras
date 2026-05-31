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
    void Run();

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

    void InitWindow();
    void DiscoverExamples();
    void SwitchToExample(int index);
    void MainLoop();
    void Cleanup();
    void HandleInput(float dt);
    void BuildUI();          // ImGui windows

    static void FramebufferResizeCallback(GLFWwindow* win, int w, int h);
    static void KeyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* win, int button, int action, int mods);
};
