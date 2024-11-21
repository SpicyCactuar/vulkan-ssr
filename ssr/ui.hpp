#pragma once

#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_window.hpp"

#include "benchmark.hpp"
#include "state.hpp"

namespace ui {
    vkutils::DescriptorPool create_descriptor_pool(const vkutils::VulkanContext& context);

    void initialise(const vkutils::VulkanWindow& vulkanWindow,
                    const vkutils::DescriptorPool& uiDescriptorPool);

    void new_frame(state::State& state, const benchmark::FrameTime& frameTime);

    void render(const vkutils::VulkanWindow& vulkanWindow,
                std::uint32_t imageIndex,
                const vkutils::Fence& uiFence,
                VkCommandBuffer uiCommandBuffer);

    void destroy();
}
