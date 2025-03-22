#pragma once

#include <utility>

#include "../vkutils/vkimage.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_window.hpp"

namespace gbuffer {
    constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    constexpr VkFormat normalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    constexpr VkFormat baseColourFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    constexpr VkFormat surfaceFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    constexpr VkFormat emissiveFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    struct GBuffer {
        GBuffer() = delete;

        explicit GBuffer(const vkutils::VulkanWindow& window,
                         const vkutils::Allocator& allocator);

        GBuffer(GBuffer&& other) noexcept;

        GBuffer& operator=(GBuffer&& other) noexcept;

        std::pair<vkutils::Image, vkutils::ImageView> depth;
        std::pair<vkutils::Image, vkutils::ImageView> normal;
        std::pair<vkutils::Image, vkutils::ImageView> baseColour;
        std::pair<vkutils::Image, vkutils::ImageView> surface;
        std::pair<vkutils::Image, vkutils::ImageView> emissive;
    };

    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context);

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               VkDescriptorSet gbufferDescriptorSet,
                               const vkutils::Sampler& screenSampler,
                               const GBuffer& gBuffer);
}
