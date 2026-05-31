#include "app.h"
#include "shared_types.h"
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

#ifndef SHADER_DIR
#define SHADER_DIR "./"
#endif

void App::run() {
    initWindow();

    m_vertSpv = std::string(SHADER_DIR) + "fullscreen.vert.spv";

    m_vk.init(m_window);
    discoverExamples();

    if (m_examples.empty())
        throw std::runtime_error("No examples found in " + std::string(SHADER_DIR) + "examples/");

    m_vk.buildPipeline(m_vertSpv, m_examples[0].fragSpv);
    m_fragWatcher = std::make_unique<ShaderWatcher>(m_examples[0].fragSpv);

    m_vk.initImGui();

    mainLoop();
    cleanup();
}

void App::initWindow() {
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(1280, 720, "zonvras | shader playground", nullptr, nullptr);
    if (!m_window) throw std::runtime_error("Failed to create GLFW window");

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
}

void App::discoverExamples() {
    std::string dir = std::string(SHADER_DIR) + "examples/";
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_directory()) continue;
        std::string name    = entry.path().filename().string();
        std::string spvPath = dir + name + "/main.frag.spv";
        if (std::filesystem::exists(spvPath))
            m_examples.push_back({name, spvPath});
    }
    std::sort(m_examples.begin(), m_examples.end(),
        [](const Example& a, const Example& b){ return a.name < b.name; });

    if (m_examples.empty()) return;
    std::cout << "[examples] Found " << m_examples.size() << " example(s):\n";
    for (auto& ex : m_examples)
        std::cout << "  - " << ex.name << "\n";
}

void App::switchToExample(int index) {
    if (index < 0 || index >= (int)m_examples.size()) return;
    if (index == m_activeExample) return;
    vkDeviceWaitIdle(m_vk.device);
    try {
        m_vk.buildPipeline(m_vertSpv, m_examples[index].fragSpv);
        m_fragWatcher = std::make_unique<ShaderWatcher>(m_examples[index].fragSpv);
        m_activeExample = index;
        std::cout << "[switch] " << m_examples[index].name << "\n";
    } catch (std::exception& e) {
        std::cerr << "[switch] Error: " << e.what() << "\n";
    }
}

void App::buildUI() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({220, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("Examples", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    for (int i = 0; i < (int)m_examples.size(); i++) {
        bool selected = (i == m_activeExample);
        if (ImGui::Selectable(m_examples[i].name.c_str(), selected))
            switchToExample(i);
        if (selected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Click window to capture mouse");
    ImGui::TextDisabled("Esc = release mouse / quit");
    ImGui::TextDisabled("WASD / QE + Shift");

    ImGui::End();
}

void App::mainLoop() {
    m_lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float  dt  = (float)(now - m_lastTime);
        m_lastTime = now;
        m_time    += dt;

        handleInput(dt);

        // Hot-reload: rebuild pipeline when the active .spv changes on disk
        if (m_fragWatcher && m_fragWatcher->changed()) {
            std::cout << "[hot-reload] " << m_examples[m_activeExample].name << "\n";
            vkDeviceWaitIdle(m_vk.device);
            try {
                m_vk.buildPipeline(m_vertSpv, m_examples[m_activeExample].fragSpv);
                std::cout << "[hot-reload] OK\n";
            } catch (std::exception& e) {
                std::cerr << "[hot-reload] Error: " << e.what() << "\n";
            }
        }

        // ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        buildUI();
        ImGui::Render();

        // Build push constants
        PushConstants pc{};
        pc.time       = m_time;
        int fw, fh;
        glfwGetFramebufferSize(m_window, &fw, &fh);
        pc.resolution = {(float)fw, (float)fh};
        pc.camPos     = m_camera.position;
        pc.camRot     = { m_camera.orientation.w,
                          m_camera.orientation.x,
                          m_camera.orientation.y,
                          m_camera.orientation.z };

        if (!m_vk.drawFrame(pc)) {
            int w, h;
            glfwGetFramebufferSize(m_window, &w, &h);
            while (w == 0 || h == 0) {
                glfwWaitEvents();
                glfwGetFramebufferSize(m_window, &w, &h);
            }
            m_vk.recreateSwapchain(w, h);
            m_vk.buildPipeline(m_vertSpv, m_examples[m_activeExample].fragSpv);
        }
    }
}

void App::handleInput(float dt) {
    float fwd = 0.f, rgt = 0.f, up = 0.f;
    bool  fast  = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    float speed = fast ? 10.f : 3.f;

    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) fwd += 1.f;
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) fwd -= 1.f;
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) rgt += 1.f;
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) rgt -= 1.f;
    if (glfwGetKey(m_window, GLFW_KEY_E) == GLFW_PRESS) up  += 1.f;
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) up  -= 1.f;

    m_camera.processKeyboard(fwd, rgt, up, dt, speed);

    if (m_captureMouse) {
        double mx, my;
        glfwGetCursorPos(m_window, &mx, &my);
        if (m_firstMouse) { m_lastMouseX = mx; m_lastMouseY = my; m_firstMouse = false; }
        float dx = (float)(mx - m_lastMouseX);
        float dy = (float)(my - m_lastMouseY);
        m_lastMouseX = mx; m_lastMouseY = my;
        m_camera.processMouse(dx, dy);
    }
}

void App::cleanup() {
    m_vk.destroy();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

// ---- static callbacks ----
void App::framebufferResizeCallback(GLFWwindow*, int, int) {}

void App::keyCallback(GLFWwindow* win, int key, int, int action, int) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (app->m_captureMouse) {
            app->m_captureMouse = false;
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetWindowShouldClose(win, GLFW_TRUE);
        }
    }
}

void App::mouseButtonCallback(GLFWwindow* win, int button, int action, int) {
    // Don't capture mouse if ImGui wants the click
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(win));
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !app->m_captureMouse) {
        app->m_captureMouse = true;
        app->m_firstMouse   = true;
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}
