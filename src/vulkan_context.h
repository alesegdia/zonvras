#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <functional>
#include "shared_types.h"

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR        caps;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

class VulkanContext {
public:
    void init(GLFWwindow* window);
    void initImGui();   // call after init()
    void destroy();

    // (Re)build the graphics pipeline with the given SPIR-V files
    void buildPipeline(const std::string& vertSpv, const std::string& fragSpv);
    void destroyPipeline();

    // Draw one frame; returns false if swapchain needs rebuild
    bool drawFrame(const PushConstants& pc);

    // Recreate swapchain (e.g. after resize)
    void recreateSwapchain(int w, int h);

    VkDevice device = VK_NULL_HANDLE;

private:
    // ---- instance / device ----
    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger  = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface         = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physDev         = VK_NULL_HANDLE;
    VkQueue                  m_graphicsQueue   = VK_NULL_HANDLE;
    VkQueue                  m_presentQueue    = VK_NULL_HANDLE;
    uint32_t                 m_graphicsFamily  = 0;
    uint32_t                 m_presentFamily   = 0;

    // ---- swapchain ----
    VkSwapchainKHR             m_swapchain       = VK_NULL_HANDLE;
    VkFormat                   m_swapFormat      = VK_FORMAT_UNDEFINED;
    VkExtent2D                 m_swapExtent      = {};
    std::vector<VkImage>       m_swapImages;
    std::vector<VkImageView>   m_swapViews;
    std::vector<VkFramebuffer> m_framebuffers;

    // ---- render pass / pipeline ----
    VkRenderPass     m_renderPass  = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout  = VK_NULL_HANDLE;
    VkPipeline       m_pipeline    = VK_NULL_HANDLE;

    // ---- commands / sync ----
    VkCommandPool                m_cmdPool       = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_cmdBuffers;
    static constexpr int MAX_FRAMES = 2;
    VkSemaphore m_imageAvailable[MAX_FRAMES] = {};
    VkSemaphore m_renderFinished[MAX_FRAMES] = {};
    VkFence     m_inFlight[MAX_FRAMES]       = {};
    int         m_currentFrame = 0;

    // ---- ImGui ----
    VkDescriptorPool m_imguiDescPool  = VK_NULL_HANDLE;
    bool             m_imguiReady     = false;
    uint32_t         m_minImageCount  = 2;

    GLFWwindow* m_window = nullptr;

    // ---- helpers ----
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain(int w, int h);
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void cleanupSwapchain();

    void initImGuiImpl();
    void destroyImGuiImpl();

    VkShaderModule loadSpirV(const std::string& path);
    SwapchainSupport querySwapchainSupport(VkPhysicalDevice dev);
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>&);
    VkPresentModeKHR   choosePresentMode(const std::vector<VkPresentModeKHR>&);
    VkExtent2D         chooseExtent(const VkSurfaceCapabilitiesKHR&, int w, int h);
};
