#pragma once

#include <filesystem>

#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_window.hpp"

namespace screenshot {
    vkutils::Buffer create_screenshot_buffer(const vkutils::VulkanWindow& window,
                                             const vkutils::Allocator& allocator);

    void record_screenshot_ready_event(VkCommandBuffer commandBuffer,
                                       const vkutils::Event& screenshotReady);

    void take_screenshot(const vkutils::VulkanWindow& window,
                         const vkutils::CommandPool& commandPool,
                         VkImage frameImage,
                         const vkutils::Allocator& allocator,
                         const vkutils::Event& screenshotReady,
                         const std::filesystem::path& screenshotPath);
}
