#pragma once

#include "../vkutils/vkimage.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_window.hpp"

namespace bloom {
    constexpr VkFormat bloomFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    struct BloomBuffer {
        BloomBuffer() = delete;

        explicit BloomBuffer(const vkutils::VulkanWindow& window, const vkutils::Allocator& allocator);

        BloomBuffer(BloomBuffer&& other) noexcept;

        BloomBuffer& operator=(BloomBuffer&& other) noexcept;

        std::pair<vkutils::Image, vkutils::ImageView> brightPassBuffer;
        std::pair<vkutils::Image, vkutils::ImageView> blurPass1Buffer;
        std::pair<vkutils::Image, vkutils::ImageView> blurPass2Buffer;
    };

    // TODO: Use
    struct BloomParameters {
        float threshold = 1.0f; // Brightness threshold
        float intensity = 1.0f; // Bloom intensity
        int gaussianRadius = 4; // Blur radius
    };

    vkutils::Pipeline create_bright_pass_pipeline(const vkutils::VulkanWindow& window,
                                                  VkRenderPass renderPass,
                                                  VkPipelineLayout pipelineLayout);

    vkutils::Pipeline create_blur_pipeline(const vkutils::VulkanWindow& window,
                                           VkRenderPass renderPass,
                                           VkPipelineLayout pipelineLayout);
}
