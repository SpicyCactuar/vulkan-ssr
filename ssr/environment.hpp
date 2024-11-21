#pragma once

#include "../vkutils/vkimage.hpp"
#include "../vkutils/vkobject.hpp"

namespace environment {
    std::pair<vkutils::Image, vkutils::ImageView> load_cube_map(const vkutils::VulkanContext& context,
                                                                const vkutils::Allocator& allocator,
                                                                const vkutils::CommandPool& loadCommandPool);

    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context);

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               VkDescriptorSet environmentDescriptorSet,
                               const vkutils::ImageView& cubeMapView,
                               const vkutils::Sampler& sampler);
}
