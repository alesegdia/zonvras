#include "vulkan_context.h"
#include <stdexcept>
#include <vector>
#include <set>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <array>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// ---- validation layers ----
#ifdef NDEBUG
static constexpr bool ENABLE_VALIDATION = false;
#else
static constexpr bool ENABLE_VALIDATION = true;
#endif

static const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};
static const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// ---- debug callback ----
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    std::cerr << "[Vulkan] " << data->pMessage << "\n";
    return VK_FALSE;
}

// ---- file helper ----
static std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) throw std::runtime_error("Cannot open file: " + path);
    size_t size = (size_t)f.tellg();
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), (std::streamsize)size);
    return buf;
}

// =============================================================================
void VulkanContext::Init(GLFWwindow* window) {
    m_window = window;
    CreateInstance();
    if (ENABLE_VALIDATION) SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    CreateSwapchain(w, h);
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
}

void VulkanContext::InitImGui() {
    InitImGuiImpl();
}

void VulkanContext::Destroy() {
    vkDeviceWaitIdle(m_device);
    if (m_imguiReady) DestroyImGuiImpl();
    for (int i = 0; i < MAX_FRAMES; i++) {
        vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
        vkDestroyFence(m_device, m_inFlight[i], nullptr);
    }
    DestroyPipeline();
    CleanupSwapchain();
    vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    vkDestroyDevice(m_device, nullptr);
    if (ENABLE_VALIDATION) {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(m_instance, m_debugMessenger, nullptr);
    }
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

// ---- pipeline ----
void VulkanContext::DestroyPipeline() {
    if (m_pipeline)   { vkDestroyPipeline(m_device, m_pipeline, nullptr);       m_pipeline  = VK_NULL_HANDLE; }
    if (m_pipeLayout) { vkDestroyPipelineLayout(m_device, m_pipeLayout, nullptr); m_pipeLayout = VK_NULL_HANDLE; }
}

void VulkanContext::BuildPipeline(const std::string& vertSpvPath, const std::string& fragSpvPath) {
    DestroyPipeline();

    VkShaderModule vertMod = LoadSpirV(vertSpvPath);
    VkShaderModule fragMod = LoadSpirV(fragSpvPath);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    // No vertex input — fullscreen triangle generated in vertex shader
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{ 0, 0, (float)m_swapExtent.width, (float)m_swapExtent.height, 0.f, 1.f };
    VkRect2D   scissor{ {0,0}, m_swapExtent };

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1; vp.pViewports = &viewport;
    vp.scissorCount  = 1; vp.pScissors  = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAtt;

    // Push constants for uniforms
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges    = &pcRange;
    if (vkCreatePipelineLayout(m_device, &layoutCI, nullptr, &m_pipeLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipeCI{};
    pipeCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeCI.stageCount          = 2;
    pipeCI.pStages             = stages;
    pipeCI.pVertexInputState   = &vertexInput;
    pipeCI.pInputAssemblyState = &ia;
    pipeCI.pViewportState      = &vp;
    pipeCI.pRasterizationState = &raster;
    pipeCI.pMultisampleState   = &ms;
    pipeCI.pColorBlendState    = &blend;
    pipeCI.layout              = m_pipeLayout;
    pipeCI.renderPass          = m_renderPass;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
}

// ---- draw ----
bool VulkanContext::DrawFrame(const PushConstants& pc) {
    vkWaitForFences(m_device, 1, &m_inFlight[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) return false;

    vkResetFences(m_device, 1, &m_inFlight[m_currentFrame]);

    VkCommandBuffer cmd = m_cmdBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clear{ .color = {.float32 = {0.f, 0.f, 0.f, 1.f}} };
    VkRenderPassBeginInfo rpBI{};
    rpBI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBI.renderPass      = m_renderPass;
    rpBI.framebuffer     = m_framebuffers[imageIndex];
    rpBI.renderArea      = { {0,0}, m_swapExtent };
    rpBI.clearValueCount = 1;
    rpBI.pClearValues    = &clear;

    vkCmdBeginRenderPass(cmd, &rpBI, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdPushConstants(cmd, m_pipeLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(PushConstants), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen triangle — no vertex buffer

    // ImGui overlay
    if (m_imguiReady)
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &m_imageAvailable[m_currentFrame];
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &m_renderFinished[m_currentFrame];
    vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlight[m_currentFrame]);

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &m_renderFinished[m_currentFrame];
    present.swapchainCount     = 1;
    present.pSwapchains        = &m_swapchain;
    present.pImageIndices      = &imageIndex;
    res = vkQueuePresentKHR(m_presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES;
        return false;
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES;
    return true;
}

// ---- swapchain recreation ----
void VulkanContext::RecreateSwapchain(int w, int h) {
    vkDeviceWaitIdle(m_device);
    CleanupSwapchain();
    CreateSwapchain(w, h);
    CreateImageViews();
    CreateFramebuffers();
    CreateCommandBuffers();
    if (m_imguiReady)
        ImGui_ImplVulkan_SetMinImageCount(m_minImageCount);
}

// =============================================================================
// Private helpers
// =============================================================================

void VulkanContext::CreateInstance() {
    if (ENABLE_VALIDATION) {
        uint32_t cnt; vkEnumerateInstanceLayerProperties(&cnt, nullptr);
        std::vector<VkLayerProperties> layers(cnt);
        vkEnumerateInstanceLayerProperties(&cnt, layers.data());
        for (auto* name : VALIDATION_LAYERS) {
            bool found = false;
            for (auto& l : layers) if (strcmp(l.layerName, name) == 0) { found = true; break; }
            if (!found) throw std::runtime_error(std::string("Validation layer not found: ") + name);
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "zonvras";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    uint32_t glfwExtCount;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwExtCount);
    if (ENABLE_VALIDATION) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();
    if (ENABLE_VALIDATION) {
        ci.enabledLayerCount   = (uint32_t)VALIDATION_LAYERS.size();
        ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    if (vkCreateInstance(&ci, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");
}

void VulkanContext::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn || fn(m_instance, &ci, nullptr, &m_debugMessenger) != VK_SUCCESS)
        std::cerr << "Warning: could not set up debug messenger\n";
}

void VulkanContext::CreateSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

void VulkanContext::PickPhysicalDevice() {
    uint32_t cnt; vkEnumeratePhysicalDevices(m_instance, &cnt, nullptr);
    if (!cnt) throw std::runtime_error("No Vulkan GPU found");
    std::vector<VkPhysicalDevice> devs(cnt);
    vkEnumeratePhysicalDevices(m_instance, &cnt, devs.data());

    for (auto dev : devs) {
        // Check queue families
        uint32_t qCnt; vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCnt, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCnt);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCnt, qProps.data());

        int gfx = -1, prs = -1;
        for (uint32_t i = 0; i < qCnt; i++) {
            if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) gfx = (int)i;
            VkBool32 sup; vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &sup);
            if (sup) prs = (int)i;
        }
        if (gfx < 0 || prs < 0) continue;

        // Check m_device extensions
        uint32_t eCnt; vkEnumerateDeviceExtensionProperties(dev, nullptr, &eCnt, nullptr);
        std::vector<VkExtensionProperties> eProps(eCnt);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &eCnt, eProps.data());
        bool allExts = true;
        for (auto* name : DEVICE_EXTENSIONS) {
            bool found = false;
            for (auto& e : eProps) if (strcmp(e.extensionName, name) == 0) { found = true; break; }
            if (!found) { allExts = false; break; }
        }
        if (!allExts) continue;

        auto sc = QuerySwapchainSupport(dev);
        if (sc.formats.empty() || sc.presentModes.empty()) continue;

        m_physDev        = dev;
        m_graphicsFamily = (uint32_t)gfx;
        m_presentFamily  = (uint32_t)prs;
        return;
    }
    throw std::runtime_error("No suitable Vulkan GPU found");
}

void VulkanContext::CreateLogicalDevice() {
    std::set<uint32_t> uniqueFamilies = { m_graphicsFamily, m_presentFamily };
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    float priority = 1.f;
    for (auto fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueCIs.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = (uint32_t)queueCIs.size();
    ci.pQueueCreateInfos       = queueCIs.data();
    ci.enabledExtensionCount   = (uint32_t)DEVICE_EXTENSIONS.size();
    ci.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    ci.pEnabledFeatures        = &features;
    if (ENABLE_VALIDATION) {
        ci.enabledLayerCount   = (uint32_t)VALIDATION_LAYERS.size();
        ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    if (vkCreateDevice(m_physDev, &ci, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical m_device");

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily,  0, &m_presentQueue);
}

SwapchainSupport VulkanContext::QuerySwapchainSupport(VkPhysicalDevice dev) {
    SwapchainSupport sc;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, m_surface, &sc.caps);
    uint32_t cnt;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &cnt, nullptr);
    sc.formats.resize(cnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &cnt, sc.formats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface, &cnt, nullptr);
    sc.presentModes.resize(cnt);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface, &cnt, sc.presentModes.data());
    return sc;
}

VkSurfaceFormatKHR VulkanContext::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& fmts) {
    for (auto& f : fmts)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return fmts[0];
}

VkPresentModeKHR VulkanContext::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, int w, int h) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    return {
        std::clamp((uint32_t)w, caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height)
    };
}

void VulkanContext::CreateSwapchain(int w, int h) {
    auto sc  = QuerySwapchainSupport(m_physDev);
    auto fmt = ChooseSurfaceFormat(sc.formats);
    auto pm  = ChoosePresentMode(sc.presentModes);
    auto ext = ChooseExtent(sc.caps, w, h);

    uint32_t imgCount = sc.caps.minImageCount + 1;
    if (sc.caps.maxImageCount > 0) imgCount = std::min(imgCount, sc.caps.maxImageCount);
    m_minImageCount = sc.caps.minImageCount + 1;
    if (sc.caps.maxImageCount > 0) m_minImageCount = std::min(m_minImageCount, sc.caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = m_surface;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = fmt.format;
    ci.imageColorSpace  = fmt.colorSpace;
    ci.imageExtent      = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t families[] = { m_graphicsFamily, m_presentFamily };
    if (m_graphicsFamily != m_presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    ci.preTransform   = sc.caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = pm;
    ci.clipped        = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    uint32_t cnt; vkGetSwapchainImagesKHR(m_device, m_swapchain, &cnt, nullptr);
    m_swapImages.resize(cnt);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &cnt, m_swapImages.data());
    m_swapFormat = fmt.format;
    m_swapExtent = ext;
}

void VulkanContext::CreateImageViews() {
    m_swapViews.resize(m_swapImages.size());
    for (size_t i = 0; i < m_swapImages.size(); i++) {
        VkImageViewCreateInfo ci{};
        ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image                           = m_swapImages[i];
        ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ci.format                          = m_swapFormat;
        ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount     = 1;
        ci.subresourceRange.layerCount     = 1;
        vkCreateImageView(m_device, &ci, nullptr, &m_swapViews[i]);
    }
}

void VulkanContext::CreateRenderPass() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format         = m_swapFormat;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &colorAtt;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;

    if (vkCreateRenderPass(m_device, &ci, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
}

void VulkanContext::CreateFramebuffers() {
    m_framebuffers.resize(m_swapViews.size());
    for (size_t i = 0; i < m_swapViews.size(); i++) {
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = m_renderPass;
        ci.attachmentCount = 1;
        ci.pAttachments    = &m_swapViews[i];
        ci.width           = m_swapExtent.width;
        ci.height          = m_swapExtent.height;
        ci.layers          = 1;
        vkCreateFramebuffer(m_device, &ci, nullptr, &m_framebuffers[i]);
    }
}

void VulkanContext::CreateCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = m_graphicsFamily;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &ci, nullptr, &m_cmdPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}

void VulkanContext::CreateCommandBuffers() {
    m_cmdBuffers.resize(MAX_FRAMES);
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = m_cmdPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES;
    if (vkAllocateCommandBuffers(m_device, &ai, m_cmdBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

void VulkanContext::CreateSyncObjects() {
    VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
    for (int i = 0; i < MAX_FRAMES; i++) {
        vkCreateSemaphore(m_device, &si, nullptr, &m_imageAvailable[i]);
        vkCreateSemaphore(m_device, &si, nullptr, &m_renderFinished[i]);
        vkCreateFence(m_device, &fi, nullptr, &m_inFlight[i]);
    }
}

void VulkanContext::CleanupSwapchain() {
    for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();
    for (auto v : m_swapViews)    vkDestroyImageView(m_device, v, nullptr);
    m_swapViews.clear();
    if (m_swapchain) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
}

VkShaderModule VulkanContext::LoadSpirV(const std::string& path) {
    auto code = readFile(path);
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(m_device, &ci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module from: " + path);
    return mod;
}

// ============================================================
//  ImGui integration
// ============================================================
void VulkanContext::InitImGuiImpl() {
    // Minimal descriptor pool just for ImGui (one combined image sampler for the font)
    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCI.maxSets       = 1;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes    = &poolSize;
    if (vkCreateDescriptorPool(m_device, &poolCI, nullptr, &m_imguiDescPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ImGui descriptor pool");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window, /*install_callbacks=*/true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance        = m_instance;
    initInfo.PhysicalDevice  = m_physDev;
    initInfo.Device          = m_device;
    initInfo.QueueFamily     = m_graphicsFamily;
    initInfo.Queue           = m_graphicsQueue;
    initInfo.DescriptorPool  = m_imguiDescPool;
    initInfo.RenderPass      = m_renderPass;
    initInfo.MinImageCount   = m_minImageCount;
    initInfo.ImageCount      = static_cast<uint32_t>(m_swapImages.size());
    initInfo.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();

    m_imguiReady = true;
}

void VulkanContext::DestroyImGuiImpl() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_device, m_imguiDescPool, nullptr);
    m_imguiDescPool = VK_NULL_HANDLE;
    m_imguiReady    = false;
}
