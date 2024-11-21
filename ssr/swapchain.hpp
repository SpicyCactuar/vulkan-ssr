#pragma once

#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_window.hpp"

#include "mesh.hpp"

namespace swapchain {
    std::vector<vkutils::Framebuffer> create_swapchain_framebuffers(const vkutils::VulkanWindow& window,
                                                                    VkRenderPass renderPass);

    std::uint32_t acquire_swapchain_image(const vkutils::VulkanWindow& vulkanWindow,
                                          const vkutils::Semaphore& imageAvailable,
                                          bool& needToRecreateSwapchain);

    void present_results(VkQueue presentQueue,
                         VkSwapchainKHR swapchain,
                         std::uint32_t imageIndex,
                         VkSemaphore renderFinished,
                         bool& needToRecreateSwapchain);
}
