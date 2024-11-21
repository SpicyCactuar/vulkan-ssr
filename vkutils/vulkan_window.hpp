#pragma once

#include <volk/volk.h>

#if !defined(GLFW_INCLUDE_NONE)
#	define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <vector>
#include <cstdint>
#include <string>

#include "vulkan_context.hpp"

namespace vkutils {
    class VulkanWindow final : public VulkanContext {
    public:
        VulkanWindow(), ~VulkanWindow();

        // Move-only
        VulkanWindow(const VulkanWindow&) = delete;

        VulkanWindow& operator=(const VulkanWindow&) = delete;

        VulkanWindow(VulkanWindow&&) noexcept;

        VulkanWindow& operator=(VulkanWindow&&) noexcept;

        GLFWwindow* window = nullptr;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        std::uint32_t presentFamilyIndex = 0;
        VkQueue presentQueue = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> swapImages;
        std::vector<VkImageView> swapViews;

        VkFormat swapchainFormat;
        VkExtent2D swapchainExtent;
    };

    VulkanWindow make_vulkan_window(unsigned int windowWidth, unsigned int windowHeight, const std::string& sceneName);

    struct SwapChanges {
        bool changedSize = false;
        bool changedFormat = false;
    };

    SwapChanges recreate_swapchain(VulkanWindow&);
}
