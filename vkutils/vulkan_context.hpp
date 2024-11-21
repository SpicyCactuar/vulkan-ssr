#pragma once

#include <cstdint>

#include <volk/volk.h>

namespace vkutils {
    class VulkanContext {
    public:
        VulkanContext(), ~VulkanContext();

        // Move-only
        VulkanContext(const VulkanContext&) = delete;

        VulkanContext& operator=(const VulkanContext&) = delete;

        VulkanContext(VulkanContext&&) noexcept;

        VulkanContext& operator=(VulkanContext&&) noexcept;

        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        VkDevice device = VK_NULL_HANDLE;

        std::uint32_t graphicsFamilyIndex = 0;
        VkQueue graphicsQueue = VK_NULL_HANDLE;

        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    };

    VulkanContext make_vulkan_context();
}
