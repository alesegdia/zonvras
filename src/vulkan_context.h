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
    void Init(GLFWwindow* window);
    void InitImGui();   // call after Init()
    void Destroy();

    // (Re)build the graphics pipeline with the given SPIR-V files
    void BuildPipeline(const std::string& vertSpv, const std::string& fragSpv);
    void DestroyPipeline();

    // Draw one frame; returns false if swapchain needs rebuild
    bool DrawFrame(const PushConstants& pc);

    // Recreate swapchain (e.g. after resize)
    void RecreateSwapchain(int w, int h);

    VkDevice m_device = VK_NULL_HANDLE;

private:
    // ---- instance / m_device ----
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
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain(int w, int h);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanupSwapchain();

    void InitImGuiImpl();
    void DestroyImGuiImpl();

    VkShaderModule LoadSpirV(const std::string& path);
    SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice dev);
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>&);
    VkPresentModeKHR   ChoosePresentMode(const std::vector<VkPresentModeKHR>&);
    VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR&, int w, int h);
};
